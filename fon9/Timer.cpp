// \file fon9/Timer.cpp
// \author fonwinz@gmail.com
#include "fon9/Timer.hpp"
//#include "fon9/impl/OnWindowsMainExit.hpp"

namespace fon9 {

TimerThread& GetDefaultTimerThread() {
#if 0
   struct DefaultTimerThread : public TimerThread, impl::OnWindowsMainExitHandle {
      fon9_NON_COPY_NON_MOVE(DefaultTimerThread);
      DefaultTimerThread() : TimerThread{"Default.TimerThread.Thread"} {}
      void OnWindowsMainExit_Notify() { this->Dispose(); }
      void OnWindowsMainExit_ThreadJoin() { ThreadJoin(*this); }
   };
#else
   struct DefaultTimerThread : public TimerThread {
      fon9_NON_COPY_NON_MOVE(DefaultTimerThread);
      DefaultTimerThread() : TimerThread{"Default.TimerThread.Thread"} {}
   };
#endif
   static DefaultTimerThread TimerThread_;
   return TimerThread_;
}

//--------------------------------------------------------------------------//

TimerEntry::~TimerEntry() {
}
void TimerEntry::DisposeAndWait() {
   for (;;) {
      TimerThread::Locker   timerThread{this->TimerThread_.TimerController_};
      if (IsTimerWaitInLine(this->Key_.SeqNo_)) {
         timerThread->Erase(this->Key_);
         this->Key_.SeqNo_ = TimerSeqNo::Disposed;
         this->Key_.EmitTime_.AssignNull();
      }
      if (!this->TimerThread_.CheckCurrEmit(timerThread, *this))
         break;
   }
}
void TimerEntry::StopAndWait() {
   for (;;) {
      TimerThread::Locker   timerThread{this->TimerThread_.TimerController_};
      if (this->Key_.SeqNo_ == TimerSeqNo::Disposed)
         break;
      timerThread->Erase(this->Key_);
      this->Key_.SeqNo_ = TimerSeqNo::NoWaiting;
      this->Key_.EmitTime_.AssignNull();
      if (!this->TimerThread_.CheckCurrEmit(timerThread, *this))
         break;
   }
}
void TimerEntry::DisposeNoWait() {
   TimerThread::Locker   timerThread{this->TimerThread_.TimerController_};
   if (IsTimerWaitInLine(this->Key_.SeqNo_))
      timerThread->Erase(this->Key_);
   this->Key_.SeqNo_ = TimerSeqNo::Disposed;
}
void TimerEntry::StopNoWait() {
   TimerThread::Locker   timerThread{this->TimerThread_.TimerController_};
   if (IsTimerWaitInLine(this->Key_.SeqNo_)) {
      timerThread->Erase(this->Key_);
      this->Key_.SeqNo_ = TimerSeqNo::NoWaiting;
   }
}
void TimerEntry::SetupRun(TimeStamp atTimePoint, const TimeInterval* after) {
   TimerThread::Locker   timerThread{this->TimerThread_.TimerController_};
   if (this->Key_.SeqNo_ == TimerSeqNo::Disposed)
      return;
   if (IsTimerWaitInLine(this->Key_.SeqNo_))
      timerThread->Erase(this->Key_);

   this->Key_.EmitTime_ = atTimePoint;
   timerThread->LastSeqNo_ = static_cast<TimerSeqNo>(static_cast<underlying_type_t<TimerSeqNo>>(timerThread->LastSeqNo_) + 1);
   if (fon9_UNLIKELY(!IsTimerWaitInLine(timerThread->LastSeqNo_)))
      timerThread->LastSeqNo_ = TimerSeqNo::WaitInLine;
   this->Key_.SeqNo_ = timerThread->LastSeqNo_;

   auto ifind = timerThread->Timers_.insert(TimerThread::TimerThreadData::Timers::value_type{this->Key_, this->shared_from_this()}).first;
   if (fon9_LIKELY(ifind != timerThread->Timers_.end() - 1))
      return;
   TimeInterval wsecs = after ? *after : TimeInterval{atTimePoint - UtcNow()};
   if (fon9_UNLIKELY(timerThread->CvWaitSecs_ == wsecs))
      return;
   timerThread->CvWaitSecs_ = wsecs;
   this->TimerThread_.TimerController_.NotifyOne(timerThread);
}
void TimerEntry::OnTimer(TimeStamp /*now*/) {
}
void TimerEntry::EmitOnTimer(TimeStamp now) {
   this->OnTimer(now);
   intrusive_ptr_release(this);
}
void TimerEntry::OnTimerEntryReleased() {
   delete this;
}

//--------------------------------------------------------------------------//

DataMemberTimer::DataMemberTimer() : DataMemberTimer(GetDefaultTimerThread()) {
}
DataMemberTimer::DataMemberTimer(TimerThread& timerThread) : TimerEntry(timerThread) {
   intrusive_ptr_add_ref(this);
}
void DataMemberTimer::OnTimerEntryReleased() {
}

//--------------------------------------------------------------------------//

TimerThread::TimerThread(std::string timerName) {
   this->TimerController_.OnBeforeThreadStart(1);
   this->Thread_ = std::thread(&TimerThread::ThrRun, this, std::move(timerName));
}
TimerThread::~TimerThread() {
   this->TimerController_.WaitTerminate();
   if (this->Thread_.joinable())
      this->Thread_.join();
}

bool TimerThread::CheckCurrEmit(Locker& timerThread, TimerEntry& timer) {
   if (timerThread->CurrEntry_ != &timer || this->InThisThread())
      return false;
   timerThread.unlock();
   std::this_thread::yield();
   return true;
}

void TimerThread::ThrRun(std::string timerName) {
   (void)timerName;//fon9_LOG_ThrRun("TimerThread.ThrRun|name=", timerName);
   for (;;) {
      Locker   timerThread{this->TimerController_};
      bool     waitResult = (timerThread->CvWaitSecs_.GetOrigValue() < 0
                              ? this->TimerController_.Wait(timerThread)
                              : this->TimerController_.WaitFor(timerThread, timerThread->CvWaitSecs_.ToDuration()));
      if (fon9_UNLIKELY(!waitResult))
         break;

      for (;;) {
         if (timerThread->Timers_.empty()) {
            timerThread->CvWaitSecs_ = TimeInterval_Second(-1);
            break;
         }
         TimerEntrySP   timer = timerThread->Timers_.back().second;
         TimeStamp      now = UtcNow();
         TimeInterval   ti = timer->Key_.EmitTime_ - now;
         if (ti.GetOrigValue() > 0) {
            timerThread->CvWaitSecs_ = ti;
            break;
         }
         timer->Key_.SeqNo_ = TimerSeqNo::NoWaiting;
         timerThread->Timers_.pop_back();
         timerThread->CurrEntry_ = timer.get();
         timerThread.unlock();
         // callback in unlock...
         // 在這兒 timer.detach(), 然後在 EmitOnTimer() 裡面 intrusive_ptr_release(timer);
         timer.detach()->EmitOnTimer(now);
         timerThread.lock();
         timerThread->CurrEntry_ = nullptr;
         if (!this->TimerController_.CheckRunning(timerThread))
            goto __BREAK_ThrRun;
      }
   }
__BREAK_ThrRun:;
   //fon9_LOG_ThrRun("TimerThread.ThrRun.End|name=", timerName);
}

} // namespace fon9
