/// \file fon9/ThreadController.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_ThreadController_hpp__
#define __fon9_ThreadController_hpp__
#include "fon9/MustLock.hpp"
#include "fon9/WaitPolicy.hpp"

namespace fon9 {

enum class ThreadState : uint8_t {
   /// thread 尚未啟動.
   Idle,
   /// thread 已啟動, 執行中的 thread 允許進入 wait 狀態.
   ExecutingOrWaiting,
   /// thread 已啟動, 執行中的 thread 不允許進入 wait 狀態.
   /// thread 必須在 [工作執行完畢] 後 [結束].
   /// 此時不能再加入新工作.
   EndAfterWorkDone,
   /// 通知 thread 應立即結束.
   EndNow,
   /// 全部的 thread 已結束.
   Terminated,
};

fon9_WARN_DISABLE_PADDING;
/// \ingroup Thrs
/// 一般 thread 執行時的資料保護、流程控制。
/// - 被保護的資料: ProtectedT
///   - 請參考 MustLock<> 機制: 必須使用 Locker lk{}; 才能取用.
/// - 使用範例可參考 MessageQueue.hpp
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

   ThreadState GetState(const Locker&) const { return this->State_; }
   uint32_t    GetRunningCount(const Locker&) const { return this->RunningCount_; }

   void OnBeforeThreadStart(uint32_t threadCount) {
      assert(this->State_ == ThreadState::Idle && this->RunningCount_ == 0);
      this->State_ = ThreadState::ExecutingOrWaiting;
      this->RunningCount_ = threadCount;
   }
   void OnBeforeThreadEnd(Locker& lk) {
      assert(this->RunningCount_ > 0);
      if (--this->RunningCount_ == 0) {
         this->State_ = ThreadState::Terminated;
         this->NotifyAll(lk);
      }
   }

   void NotifyForEndNow(Locker& lk) { this->NotifyForEnd(lk, ThreadState::EndNow); }
   void NotifyForEndNow() { this->NotifyForEnd(ThreadState::EndNow); }
   void WaitForEndNow(Locker& lk) { this->WaitForEnd(lk, ThreadState::EndNow); }
   void WaitForEndNow() { this->WaitForEnd(ThreadState::EndNow); }

   void NotifyForEndAfterWorkDone(Locker& lk) { this->NotifyForEnd(lk, ThreadState::EndAfterWorkDone); }
   void NotifyForEndAfterWorkDone() { this->NotifyForEnd(ThreadState::EndAfterWorkDone); }
   void WaitForEndAfterWorkDone(Locker& lk) { this->WaitForEnd(lk, ThreadState::EndAfterWorkDone); }
   void WaitForEndAfterWorkDone() { this->WaitForEnd(ThreadState::EndAfterWorkDone); }

   void Wait(Locker& lk) {
      if (this->State_ == ThreadState::ExecutingOrWaiting)
         this->WaitPolicy_.Wait(lk);
   }
   template<class Duration>
   void WaitFor(Locker& lk, const Duration& dur) {
      if (this->State_ == ThreadState::ExecutingOrWaiting)
         this->WaitPolicy_.WaitFor(lk, dur);
   }

   void NotifyOne(Locker& lk) {
      this->WaitPolicy_.NotifyOne(lk);
   }
   void NotifyAll(Locker& lk) {
      this->WaitPolicy_.NotifyAll(lk);
   }

private:
   void NotifyForEnd(Locker& lk, ThreadState st) {
      assert(st == ThreadState::EndNow || st == ThreadState::EndAfterWorkDone);
      if (this->State_ == ThreadState::ExecutingOrWaiting)
         this->State_ = st;
      this->NotifyAll(lk);
   }
   void NotifyForEnd(ThreadState st) {
      Locker lk{*this};
      this->NotifyForEnd(lk, st);
   }
   void WaitForEnd(Locker& lk, ThreadState st) {
      this->NotifyForEnd(lk, st);
      while (this->RunningCount_ > 0)
         this->WaitPolicy_.Wait(lk);
   }
   void WaitForEnd(ThreadState st) {
      Locker lk{*this};
      this->WaitForEnd(lk, st);
   }
};
fon9_WARN_POP;

} // namespace fon9
#endif//__fon9_ThreadController_hpp__
