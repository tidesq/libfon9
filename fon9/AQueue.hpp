/// \file fon9/AQueue.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_AQueue_hpp__
#define __fon9_AQueue_hpp__
#include "fon9/Worker.hpp"
#include "fon9/SpinMutex.hpp"
#include "fon9/CountDownLatch.hpp"
#include <vector>

namespace fon9 {

/// \ingroup Thrs
/// AQueue Lock 之後, 想要優先執行的工作種類.
/// - 使用 bit 方式設定工作種類.
/// - Lock 時, 提供工作種類(某 bit 為 1):
///   - 如果此時 TaskQueue 正在運作: 
///     - 將新工作放入 TaskQueue.
///   - 如果此時 TaskQueue 沒有在運作:
///     - 設定該 bit
///     - unlock
///     - 立即執行該種類的工作
///     - 重新 lock, 然後:
///       - 清除該 bit
///       - 若 TaskQueue 不是 empty():
///         - 表示在剛剛「unlock 之後 lock 之前」有新加入的工作.
///         - 應啟動 MakeCallForWork();
/// - 底下任何一個條件成立, 就表示 TaskQueue 正在運作:
///   - TaskQueue 不是 empty()
///   - 正在執行 TaskQueue 的 thread 不是 this_thread
///     - WorkingThread != 0 && WorkingThread != ThisThread
///     - WorkingThread == ThreadThread: 雖然 TaskQueue 正在執行, 但不算是正在運作.
///   - 特定工作種類的 bit 為 1
enum class AQueueTaskKind {
   Empty = 0,

   /// 取得被保護物件的資料, 允許重複進入 Get, 但如果有設定 DenyGet 則會禁止進入 Get.
   Get = 0x01,
   /// 在進入 Get 之前, 會先檢查 DenyGet 是否有被設定, 如果有, 則 Get 必須排隊.
   DenyGet = 0x02 | Get,
   /// 簡單設定被保護物件的資料.
   /// 要立即可進行設定, 必須同時沒有任何人正在 Get 狀態.
   /// 如果是複雜的操作, 應該直接加到 AQueue::AddTask()
   Set = 0x04 | DenyGet,

   Send = 0x010,
   Recv = 0x020,
};
fon9_ENABLE_ENUM_BITWISE_OP(AQueueTaskKind);

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
class AQueue {
   fon9_NON_COPY_NON_MOVE(AQueue);

   struct WorkContent : public WorkContentBase {
      AQueueTaskKind    WorkingTaskKind_{AQueueTaskKind::Empty};
      unsigned          CountGetter_{0};
      TaskContainer     Tasks_;

      bool CheckAndSet(AQueueTaskKind taskKind) {
         if (this->IsTakingCall() || !this->Tasks_.empty())
            return false;
         if (fon9_UNLIKELY(taskKind == AQueueTaskKind::Get)) {
            if (IsEnumContains(this->WorkingTaskKind_, AQueueTaskKind::DenyGet))
               return false;
            if (++this->CountGetter_ > 1) {
               assert((this->WorkingTaskKind_ & AQueueTaskKind::DenyGet) == AQueueTaskKind::Get);
               return true;
            }
         }
         else if (IsEnumContainsAny(this->WorkingTaskKind_, taskKind))
            return false;

         this->WorkingTaskKind_ |= taskKind;
         return true;
      }
      void ClearTaskKind(AQueueTaskKind taskKind) {
         if (fon9_UNLIKELY(taskKind == AQueueTaskKind::Get)) {
            assert(this->CountGetter_ > 0);
            if (--this->CountGetter_ > 0)
               return;
         }
         this->WorkingTaskKind_ -= taskKind;
      }
   };
   using WorkController = MustLock<WorkContent, Mutex>;
   using WorkLocker = typename WorkController::Locker;
   WorkController WorkController_;
   TaskInvoker    TaskInvoker_;

public:
   template <class... ArgsT>
   AQueue(ArgsT&&... args) : TaskInvoker_{std::forward<ArgsT>(args)...} {
   }
   AQueue() {}
   ~AQueue() = default;

   static AQueue& StaticCast(TaskInvoker& invoker) {
      return ContainerOf(invoker, &AQueue::TaskInvoker_);
   }

   class ALockerBase {
      fon9_NON_COPY_NON_MOVE(ALockerBase);

      friend class AQueue;
      /// 在 AQueue::WaitInvoke() 裡面使用,
      /// 解鎖後, this 不可再使用, 所以最多只能呼叫一次.
      void DangerUnlock() {
         if (this->IsAllowInvoke_)
            return;
         if (this->AddCount_) {
            this->Owner_.AfterAddTask(this->Worker_);
            this->AddCount_ = 0;
         }
         this->Worker_.unlock();
      }

   protected:
      AQueue&     Owner_;
      WorkLocker  Worker_;
      unsigned    AddCount_{0};

      ALockerBase(AQueue& owner, AQueueTaskKind taskKind)
         : Owner_(owner)
         , Worker_{owner.WorkController_}
         , InTakingCallThread_{Worker_->InTakingCallThread()}
         , IsAllowInvoke_{InTakingCallThread_ || Worker_->CheckAndSet(taskKind)}
         , TaskKind_{taskKind} {
      }

   public:
      const bool           InTakingCallThread_;
      const bool           IsAllowInvoke_;
      const AQueueTaskKind TaskKind_;

      ~ALockerBase() {
         if (this->IsAllowInvoke_) {
            if (!this->InTakingCallThread_) {
               this->Worker_.lock();
               this->Worker_->ClearTaskKind(this->TaskKind_);
               if (!this->Worker_->Tasks_.empty())
                  this->Owner_.AfterAddTask(this->Worker_);
            }
         }
         else if (this->AddCount_ > 0)
            this->Owner_.AfterAddTask(this->Worker_);
      }
      /// 如果 this->IsAllowInvoke_ == true; 則會立即執行 task.
      /// 您也可以自己先判斷 this->IsAllowInvoke_ 再決定是否呼叫 AddTask()
      void AddTask(Task task) {
         if (this->IsAllowInvoke_)
            this->Owner_.TaskInvoker_.Invoke(task);
         else {
            this->Worker_->Tasks_.emplace_back(std::move(task));
            ++this->AddCount_;
         }
      }
   };

   class ALocker : public ALockerBase {
      fon9_NON_COPY_NON_MOVE(ALocker);
   public:
      ALocker(AQueue& owner, AQueueTaskKind taskKind)
         : ALockerBase{owner, taskKind} {
         if (this->IsAllowInvoke_)
            this->Worker_.unlock();
      }
   };

   /// 針對不須立即處理的工作, 透過這裡放到 Queue 裡面, 然後在 work thread 裡面執行.
   /// 一旦有工作放到 Queue, 在還沒執行完畢前, 後續所有的工作都需放到 Queue.
   /// 直到 Queue 執行完畢, 才能再進行 ASAP 的處理.
   void AddTask(Task task) {
      WorkLocker worker(this->WorkController_);
      worker->Tasks_.emplace_back(std::move(task));
      this->AfterAddTask(worker);
   }

   /// if (Locker::IsAllowInvoke_) 則立即執行 task, 否則等候 work thread 執行完畢才返回.
   /// TaskInvoker 必須額外提供:
   /// \code
   /// Task MakeWaiterTask(Task& task, CountDownLatch& waiter) {
   ///   return Task{[&](可能需要的參數) {
   ///         this->Invoke(task);
   ///         waiter.ForceWakeUp();
   ///      }};
   /// }
   /// \endcode
   void WaitInvoke(AQueueTaskKind taskKind, Task task) {
      ALocker locker{*this, taskKind};
      if (locker.IsAllowInvoke_) {
         this->TaskInvoker_.Invoke(task);
         return;
      }
      CountDownLatch waiter{1};
      locker.AddTask(this->TaskInvoker_.MakeWaiterTask(task, waiter));
      locker.DangerUnlock(); // wait() 之前, 必須先解鎖, 否則可能死結!
      waiter.Wait();
   }

   void TakeCall() {
      TaskContainer  tasks;
      WorkLocker     worker(this->WorkController_);
      if (worker->WorkingTaskKind_ != AQueueTaskKind::Empty || worker->IsTakingCall())
         return;
      tasks.swap(worker->Tasks_);
      worker->SetTakingCallThreadId();
      worker->SetWorkerState(WorkerState::Working);
      // -----------
      worker.unlock();
      for (auto& task : tasks)
         this->TaskInvoker_.Invoke(task);
      worker.lock();
      // -----------
      worker->ClrTakingCallThreadId();
      // 如果 Tasks_ 還有剩餘的工作, 則再加入 work thread 一次.
      if (worker->Tasks_.empty())
         worker->SetWorkerState(WorkerState::Sleeping);
      else {
         worker->SetWorkerState(WorkerState::Ringing);
         this->TaskInvoker_.MakeCallForWork(worker);
      }
   }

private:
   void AfterAddTask(WorkLocker& worker) {
      if (worker->WorkingTaskKind_ == AQueueTaskKind::Empty)
         if (worker->SetToRinging())
            this->TaskInvoker_.MakeCallForWork(worker);
   }
};
fon9_WARN_POP;

} // namespace fon9
#endif//__fon9_AQueue_hpp__
