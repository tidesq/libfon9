// \file fon9/Timer_UT.cpp
// \author fonwinz@gmail.com
#include "fon9/Timer.hpp"
#include "fon9/TestTools.hpp"
#include "fon9/RevPrint.hpp"

// 壓力測試方法:
// - 建立4個 thread
// - 每個 thread 每隔 kMakeInterval, 建立一個 MySession, 共建立 kMakeTimes 個 MySession.
// - 每個 MySession 啟動 kFlowInterval 計時, 收到 kFlowTimes 次 OnTimer 之後不再繼續計時.
// - 全部 MySession 死亡後, 查看 gOnTimerTimes 是否正確
std::atomic<uint64_t>            gOnTimerTimes, gSessionCount, gSessionDtor,
                                 gUnderOnTimer, gOverOnTimer, gOverBegin, gDtorInTimerThread;
static const fon9::TimeInterval  kFlowInterval = fon9::TimeInterval_Millisecond(500);
static const uint32_t            kFlowTimes = 10;
static const fon9::TimeInterval  kMakeInterval = fon9::TimeInterval_Millisecond(1);
//每個 thread 執行約15秒.
static const uint64_t            kMakeTimes = static_cast<uint64_t>(fon9::TimeInterval_Second(15).GetOrigValue() / kMakeInterval.GetOrigValue());
static fon9::TimerThread*        gTimerThread;

//--------------------------------------------------------------------------//

fon9_WARN_DISABLE_PADDING;
template <class ThisSP>
class TimerThreadTester {
protected:
   ThisSP            ThisSP_;
   uint32_t          OnTimerCount_{};
   fon9::TimeStamp   PrevTime_{fon9::UtcNow()};
   void OnTimer(fon9::TimerEntry* timer, fon9::TimeStamp now) {
      fon9::TimeInterval ti = now - this->PrevTime_ - kFlowInterval;
      if (ti < fon9::TimeInterval_Millisecond(-1)) {
         // 比預期時間短?
         // 1.系統時間改變,
         // 2.時間精確度不足,
         ++gUnderOnTimer;
         fon9::RevBufferFixedSize<1024> rbuf;
         fon9::RevPrint(rbuf, "\n" "Under time interval=", ti, "\n\0");
         std::cout << rbuf.GetCurrent();
      }
      else if (ti > fon9::TimeInterval_Millisecond(20)) {
         ++gOverOnTimer;
         // 比預期時間長?
         // 1.系統時間改變,
         // 2.系統暫停(例:進入睡眠)後喚醒,
         // 3.OnTimer()處理時間過長: 當累計到多少 Session, 處理超過 20 ms?
         if (gOverBegin == 0)
            gOverBegin = gSessionCount - gSessionDtor;
      }
      ++gOnTimerTimes;
      if (++this->OnTimerCount_ >= kFlowTimes)
         this->ThisSP_.reset();
      else {
         this->PrevTime_ = fon9::UtcNow();
         timer->RunAfter(kFlowInterval); // 如果有需要計時, 則需要再啟動一次.
      }
   }
public:
   TimerThreadTester() {
      ++gSessionCount;
   }
   ~TimerThreadTester() {
      ++gSessionDtor;
      if (gTimerThread->InThisThread())
         ++gDtorInTimerThread;
   }
};

//--------------------------------------------------------------------------//

class MySession : public std::enable_shared_from_this<MySession>
                , public TimerThreadTester<std::shared_ptr<void>> {
   fon9_NON_COPY_NON_MOVE(MySession);

   void EmitOnTimer(fon9::TimerEntry* timer, fon9::TimeStamp now) {
      this->OnTimer(timer, now);
   }
   using FlowTimer = fon9::TimerEntry_OwnerWP<MySession, &MySession::EmitOnTimer>;
   fon9::TimerEntrySP FlowTimer_;

   MySession() = default;
public:
   static std::shared_ptr<MySession> Make(fon9::TimerThread& timerThread) {
      std::shared_ptr<MySession> res{new MySession{}};
      res->ThisSP_ = res;
      res->FlowTimer_.reset(new FlowTimer(timerThread,res));// 建立計時器.
      res->FlowTimer_->RunAfter(kFlowInterval);             // 啟動計時器.
      return res;
   }
};

/// 測試: 使用 [包含一個 TimerEntry member].
class SessionContainTimer : public fon9::intrusive_ref_counter<SessionContainTimer>
                          , public TimerThreadTester<fon9::intrusive_ptr<SessionContainTimer>>
{
   fon9_NON_COPY_NON_MOVE(SessionContainTimer);

   static void EmitOnTimer(fon9::TimerEntry* timer, fon9::TimeStamp now) {
      SessionContainTimer& rthis = fon9::ContainerOf(*static_cast<decltype(SessionContainTimer::Timer_)*>(timer), &SessionContainTimer::Timer_);
      rthis.OnTimer(timer, now);
   }
   fon9::DataMemberEmitOnTimer<&SessionContainTimer::EmitOnTimer> Timer_;

   SessionContainTimer(fon9::TimerThread& timerThread) : Timer_(timerThread) {
   }
public:
   ~SessionContainTimer() {
      this->Timer_.DisposeAndWait();
   }
   static fon9::intrusive_ptr<SessionContainTimer> Make(fon9::TimerThread& timerThread) {
      fon9::intrusive_ptr<SessionContainTimer> res{new SessionContainTimer{timerThread}};
      res->ThisSP_ = res;
      res->Timer_.RunAfter(kFlowInterval);  // 啟動計時器.
      return res;
   }
};
fon9_WARN_POP;

//--------------------------------------------------------------------------//

void TestTimerThread() {
   fon9::TimerThread timerThread{"TestTimerThread"};
   gTimerThread = &timerThread;

   std::thread thrs[4];
   size_t      thrL = 0;
   for (; thrL < fon9::numofele(thrs)/2; ++thrL)
      thrs[thrL] = std::thread([&timerThread]() {
                  std::vector<std::shared_ptr<void>> vec;
                  vec.reserve(kMakeTimes);
                  for (auto L = kMakeTimes; L > 0; --L) {
                     vec.push_back(MySession::Make(timerThread));
                     std::this_thread::sleep_for(kMakeInterval.ToDuration());
                  }
               });
   for (; thrL < fon9::numofele(thrs); ++thrL)
      thrs[thrL] = std::thread([&timerThread]() {
                  std::vector<fon9::intrusive_ptr<SessionContainTimer>> vec;
                  vec.reserve(kMakeTimes);
                  for (auto L = kMakeTimes; L > 0; --L) {
                     vec.push_back(SessionContainTimer::Make(timerThread));
                     std::this_thread::sleep_for(kMakeInterval.ToDuration());
                  }
               });

   fon9::RevBufferFixedSize<1024> rbuf;
   uint64_t cur;
   for (;;) {
      uint64_t dtor = gSessionDtor;
      cur = gSessionCount;
      rbuf.RewindEOS();
      fon9::RevPrint(rbuf, "\r" "Session count=", cur,
                     "|dtor=", dtor,
                     "|dtorInTimer=", gDtorInTimerThread.load(),
                     "|onTimer=", gOnTimerTimes.load(),
                     "|under=", gUnderOnTimer.load(),
                     "|over=", gOverOnTimer.load(),
                     "|overBegin=", gOverBegin.load());
      std::cout << rbuf.GetCurrent();
      std::cout.flush();
      if (cur >= fon9::numofele(thrs) * kMakeTimes && dtor >= cur)
         break;
      std::this_thread::sleep_for(std::chrono::seconds{1});
   }
   for (std::thread& thr : thrs)
      thr.join();
   if (cur == gSessionDtor && gOnTimerTimes == cur*kFlowTimes) {
      std::cout << "\n" "Success" << std::endl;
      return;
   }
   rbuf.RewindEOS();
   fon9::RevPrint(rbuf, "\n" "Expect  count=", cur,
                  "|dtor=", cur,
                  "|onTimer=", cur*kFlowTimes,
                  "\n" "counter not expected!");
   std::cout << rbuf.GetCurrent() << std::endl;
   abort();
}

//--------------------------------------------------------------------------//

int main() {
   fon9::AutoPrintTestInfo utinfo{"Timer"};
   TestTimerThread();

   // 測試在 main() 結束後, DefaultTimerThread 是否能正常結束.
   fon9::GetDefaultTimerThread();
}
