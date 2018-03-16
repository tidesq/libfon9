/// \file fon9/ThreadController.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_ThreadController_hpp__
#define __fon9_ThreadController_hpp__
#include "fon9/MustLock.hpp"
#include "fon9/WaitPolicy.hpp"

namespace fon9 {

enum class ThreadState : uint8_t {
   Idle,
   Running,
   Terminating,
   Terminated,
};

fon9_WARN_DISABLE_PADDING;
/// \ingroup Thrs
/// 一般 thread 執行時的資料保護、流程控制。
/// - 被保護的資料: ProtectedT
///   - 請參考 MustLock<> 機制: 必須使用 Locker lk{}; 才能取用.
/// - 啟動細節、使用範例, 可參考 ThreadController_UT.cpp
/// - 在 thread 裡面, 可以這樣使用:
/// \code
///   using MyQueue = std::queue<int>; // 需要保護(lock之後才能使用)的資料.
///   using MyQueueController = fon9::ThreadController<MyQueue, fon9::WaitPolicy_SpinBusy>;
///
///   // thread 執行的進入點.
///   void ThrRun(std::string thrName, MyQueueController& myQueueController) {
///      fon9_LOG_ThrRun("MyQueueThread.ThrRun:|name=", thrName);
///      for (;;) {
///         MyQueueController::Locker  myQueue{myQueueController};
///         if (!myQueueController.Wait(myQueue))
///            break;
///         while (!myQueue->empty()) {
///            int value = myQueue->front();
///            myQueue->pop();
///            myQueue.unlock();
///            // ... process value ...
///            myQueue.lock();
///            if (!myQueueController.CheckRunning(myQueue))
///               return;
///         }
///      }
///   }
/// \endcode
template <class ProtectedT, class WaitPolicy>
class ThreadController : public MustLock<ProtectedT, typename WaitPolicy::Mutex, typename WaitPolicy::Locker> {
   fon9_NON_COPY_NON_MOVE(ThreadController);
   using base = MustLock<ProtectedT, typename WaitPolicy::Mutex, typename WaitPolicy::Locker>;
   ThreadState State_{ThreadState::Idle};
   uint32_t    RunningCount_{0};
   WaitPolicy  WaitPolicy_;

public:
   using Locker = typename base::Locker;
   using base::base;
   ThreadController() {
   }

   void OnBeforeThreadStart(uint32_t threadCount) {
      assert(this->State_ == ThreadState::Idle && this->RunningCount_ == 0);
      this->State_ = ThreadState::Running;
      this->RunningCount_ = threadCount;
   }
   void OnThreadTerminate(Locker& lk) {
      assert(this->RunningCount_ > 0);
      if (--this->RunningCount_ == 0) {
         this->State_ = ThreadState::Terminated;
         this->NotifyAll(lk);
      }
   }

   void Terminate(Locker& lk) {
      if (this->State_ == ThreadState::Running)
         this->State_ = ThreadState::Terminating;
      this->NotifyAll(lk);
   }
   void Terminate() {
      Locker lk{*this};
      this->Terminate(lk);
   }
   void WaitTerminate(Locker& lk) {
      this->Terminate(lk);
      while (this->RunningCount_ > 0)
         this->WaitPolicy_.Wait(lk);
   }
   void WaitTerminate() {
      Locker lk{*this};
      this->WaitTerminate(lk);
   }

   ThreadState GetState(const Locker&) const {
      return this->State_;
   }
   bool CheckRunning(Locker& lk) {
      if (this->State_ == ThreadState::Running)
         return true;
      this->OnThreadTerminate(lk);
      return false;
   }

   bool Wait(Locker& lk) {
      if (this->CheckRunning(lk)) {
         this->WaitPolicy_.Wait(lk);
         return this->CheckRunning(lk);
      }
      return false;
   }
   template<class Duration>
   bool WaitFor(Locker& lk, const Duration& dur) {
      if (this->CheckRunning(lk)) {
         this->WaitPolicy_.WaitFor(lk, dur);
         return this->CheckRunning(lk);
      }
      return false;
   }

   void NotifyOne(Locker& lk) {
      this->WaitPolicy_.NotifyOne(lk);
   }
   void NotifyAll(Locker& lk) {
      this->WaitPolicy_.NotifyAll(lk);
   }
};
fon9_WARN_POP;

} // namespace fon9
#endif//__fon9_ThreadController_hpp__
