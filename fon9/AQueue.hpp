/// \file fon9/AQueue.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_AQueue_hpp__
#define __fon9_AQueue_hpp__
#include "fon9/ThreadId.hpp"
#include "fon9/MustLock.hpp"
#include "fon9/SpinMutex.hpp"
#include "fon9/CountDownLatch.hpp"
#include <vector>

namespace fon9 {

fon9_WARN_DISABLE_PADDING;
/// \ingroup Thrs
/// As soon as possible task queue.
/// - 要解決的問題:
///   - 操作有順序性，必須依序處理。
///   - 某些不急迫的操作(例:關閉通訊設備), 可以安排在其他 thread 依序處理.
///   - 如果只是要取得簡單的資料, 則可以 lock 後立即處理, 例如: Device::WaitGetDeviceId();
/// - 解決方法:
///   - 每個通訊設備擁有一個 AQueue
///   - lock AQueue 之後可以判斷是否允許 **立即** 執行.
/// - 用法範例可參考 fon9/io/DeviceAsyncOp.hpp
///
/// \code
/// struct TaskInvoker {
///   /// - 把 this->TakeCall 加入 work thread.
///   /// - 須確保在 this->TakeCall() 執行前 AQueue 不能被解構.
///   void MakeCallForWork();
///   /// - 執行 task.
///   void Invoke(Task& task);
/// };
/// \endcode
template <class Task, class TaskInvoker, class TaskContainer = std::vector<Task>, class Mutex = SpinBusy>
class AQueue : private TaskInvoker {
   fon9_NON_COPY_NON_MOVE(AQueue);

   struct WorkContent {
      TaskContainer     Tasks_;
      ThreadId::IdType  WorkingThreadId_{};
      bool IsWorking() const {
         return this->WorkingThreadId_ != ThreadId::IdType{};
      }
      void SetWorking() {
         assert(this->WorkingThreadId_ == ThreadId::IdType{});
         this->WorkingThreadId_ = GetThisThreadId().ThreadId_;
      }
      void ClrWorking() {
         assert(this->WorkingThreadId_ == GetThisThreadId().ThreadId_);
         this->WorkingThreadId_ = ThreadId::IdType{};
      }
   };
   using WorkController = MustLock<WorkContent, Mutex>;
   using LockGuard = typename WorkController::Locker;
   WorkController WorkController_;

public:
   using TaskInvoker::TaskInvoker;
   AQueue() {
   }
   ~AQueue() = default;

   static AQueue& StaticCast(TaskInvoker& invoker) {
      return *static_cast<AQueue*>(&invoker);
   }
   /// 在 Locker 解構之前, 從其他 thread 來的工作都會放到 Queue.
   class Locker {
      fon9_NON_COPY_NON_MOVE(Locker);
      AQueue&     Owner_;
      unsigned    PushedCount_{0};
      LockGuard   LockGuard_;
      /// 解鎖後, this 不可再使用!
      /// 最多只能呼叫一次.
      void DangerUnlock() {
         if (this->IsAllowInvoke_)
            return;
         if (this->PushedCount_) {
            this->Owner_.AfterPushed(this->LockGuard_, this->PushedCount_);
            this->PushedCount_ = 0;
         }
         this->LockGuard_.unlock();
      }
      friend class AQueue;
   public:
      const bool  InWorkingThread_;
      /// 是否允許立即處理: (A && B) || C
      /// - A. 沒有任何 thread 正在處理 task
      /// - B. Queue 為空 = 沒有任何等待處理的 task
      /// - C. this thread 是正在處理 tasks 的 working thread 裡面
      const bool  IsAllowInvoke_;

      Locker(AQueue& owner)
         : Owner_(owner)
         , LockGuard_{owner.WorkController_}
         , InWorkingThread_{LockGuard_->WorkingThreadId_ == GetThisThreadId().ThreadId_}
         , IsAllowInvoke_{InWorkingThread_ || (!LockGuard_->IsWorking() && LockGuard_->Tasks_.empty())} {
         if (this->IsAllowInvoke_) {
            if (!this->InWorkingThread_)
               this->LockGuard_->SetWorking();
            this->LockGuard_.unlock();
         }
      }
      ~Locker() {
         if (this->IsAllowInvoke_) {
            if (!this->InWorkingThread_)
               this->Owner_.AfterWorkDone();
         }
         else {
            if (this->PushedCount_)
               this->Owner_.AfterPushed(this->LockGuard_, this->PushedCount_);
         }
      }
      /// 如果 this->IsAllowInvoke_ == true; 則會立即執行 task().
      /// 您也可以自己先判斷 this->IsAllowInvoke_ 再決定是否呼叫 AddTask()
      void AddTask(Task task) {
         if (this->IsAllowInvoke_)
            this->Owner_.Invoke(task);
         else {
            this->LockGuard_->Tasks_.emplace_back(std::move(task));
            ++this->PushedCount_;
         }
      }
   };
   /// 針對不須立即處理的工作, 透過這裡放到 Queue 裡面, 然後在 work thread 裡面執行.
   /// 一旦有工作放到 Queue, 在還沒執行完畢前, 後續所有的工作都需放到 Queue.
   /// 直到 Queue 執行完畢, 才能再進行 ASAP 的處理.
   void AddTask(Task task) {
      LockGuard locker(this->WorkController_);
      locker->Tasks_.emplace_back(std::move(task));
      this->AfterPushed(locker, 1);
   }

   /// - if (Locker::IsAllowInvoke_) 則立即執行 task, 否則等候 work thread 執行完畢才返回.
   /// TaskInvoker 必須額外提供:
   /// \code
   /// Task MakeWaiterTask(Task& task, CountDownLatch& waiter) {
   ///   return Task{[&](可能需要的參數) {
   ///         this->Invoke(task);
   ///         waiter.ForceWakeUp();
   ///      }};
   /// }
   /// \endcode
   void WaitInvoke(Task task) {
      Locker locker{*this};
      if (locker.IsAllowInvoke_) {
         this->Invoke(task);
         return;
      }
      CountDownLatch waiter{1};
      locker.AddTask(this->MakeWaiterTask(task, waiter));
      locker.DangerUnlock(); // wait() 之前, 必須先解鎖, 否則可能死結!
      waiter.Wait();
   }

   void TakeCall() {
      TaskContainer tasks;
      {  // lock & check.
         LockGuard locker(this->WorkController_);
         if (locker->IsWorking())
            return;
         tasks.swap(locker->Tasks_);
         locker->SetWorking();
      }  // unlock.
      for (auto& task : tasks)
         this->Invoke(task);
      this->AfterWorkDone();
   }

private:
   void AfterPushed(LockGuard& locker, unsigned pushedCount) {
      if (pushedCount <= 0 || (locker->Tasks_.size() - pushedCount) > 0 || locker->IsWorking())
         return;
      locker.unlock();
      this->MakeCallForWork();
   }

   /// - 清除工作狀態
   /// - 如果 Tasks_ 還有剩餘的工作, 則再加入 work thread 一次.
   void AfterWorkDone() {
      {  // lock & check.
         LockGuard locker(this->WorkController_);
         assert(locker->IsWorking());
         locker->ClrWorking();
         if (locker->Tasks_.empty())
            return;
      }  // unlock.
      this->MakeCallForWork();
   }
};
fon9_WARN_POP;

} // namespace fon9
#endif//__fon9_AQueue_hpp__
