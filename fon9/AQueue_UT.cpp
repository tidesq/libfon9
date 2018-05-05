// \file fon9/MPSC_LatencyUT.cpp
// \author fonwinz@gmail.com
#include "fon9/AQueue.hpp"
#include "fon9/CyclicBarrier.hpp"
#include "fon9/MessageQueue.hpp"
#include "fon9/TestTools.hpp"

//--------------------------------------------------------------------------//

struct AQueueHandler {
   using MessageType = std::function<void()>;
   using ThreadPool = fon9::MessageQueue<AQueueHandler>;
   uint64_t MsgCount_{0};
   AQueueHandler(ThreadPool&) {}
   AQueueHandler() {}
   void OnMessage(MessageType& task) {
      ++this->MsgCount_;
      task();
   }
   void OnThreadEnd(const std::string& threadName) {
      fon9_LOG_INFO("AQueueHandler.End|name=", threadName, "|messageCount=", this->MsgCount_);
   }
};
using ThreadPool = AQueueHandler::ThreadPool;

using AQueueTask = std::function<void()>;
struct AQueueInvoker {
   fon9_NON_COPY_NON_MOVE(AQueueInvoker);
   using AQueue = fon9::AQueue<AQueueTask, AQueueInvoker>;

   ThreadPool& Consumer_;
   AQueueInvoker(ThreadPool& consumer) : Consumer_(consumer) {
   }
   template <class Locker>
   void MakeCallForWork(Locker& worker) {
      worker.unlock();
      this->Consumer_.EmplaceMessage([this]() {
         AQueue::StaticCast(*this).TakeCall();
      });
   }
   void Invoke(AQueueTask& task) {
      task();
   }
   AQueueTask MakeWaiterTask(AQueueTask& task, fon9::CountDownLatch& waiter) {
      return AQueueTask{[&]() {
         this->Invoke(task);
         waiter.ForceWakeUp();
      }};
   }
};
using AQueue = AQueueInvoker::AQueue;

//--------------------------------------------------------------------------//

static const uint64_t      kTimes = 1000 * 1000;
static std::atomic<bool>   IsRunning_{false};
static uint64_t            RunCount_{0};

struct AQueueTestTask {
   void operator()() {
      bool exp = false;
      if (!IsRunning_.compare_exchange_strong(exp, true, std::memory_order_acquire)) {
         std::cout << "task reenter?!" << std::endl;
         abort();
      }
      //std::this_thread::sleep_for(std::chrono::milliseconds(1));
      ++RunCount_;
      IsRunning_.store(false, std::memory_order_release);
   }
};
struct RunAQueueBase {
   fon9_NON_COPY_NON_MOVE(RunAQueueBase);
   AQueue&              AQueue_;
   fon9::CyclicBarrier& StartingLine_;
   RunAQueueBase(AQueue& qu, fon9::CyclicBarrier& startingLine) : AQueue_(qu), StartingLine_(startingLine) {
   }
};
fon9_WARN_DISABLE_PADDING;
fon9_MSC_WARN_DISABLE_NO_PUSH(4355);// 'this': used in base member initializer list.
struct RunASAP : public RunAQueueBase {
   fon9_NON_COPY_NON_MOVE(RunASAP);
   uint64_t    CountImmediate_{}, CountQueued_{};
   std::thread Thread_;
   fon9::AQueueTaskKind TaskKind_;
   RunASAP(AQueue& qu, fon9::CyclicBarrier& thrBarrier, fon9::AQueueTaskKind taskKind)
      : RunAQueueBase{qu, thrBarrier}
      , Thread_{&RunASAP::ThrRun, this}
      , TaskKind_{taskKind} {
   }
   ~RunASAP() {
      this->Thread_.join();
   }
   void ThrRun() {
      this->StartingLine_.Wait();
      AQueueTestTask task;
      for (uint64_t L = 0; L < kTimes; ++L) {
         AQueue::ALockerForInvoke  alocker(this->AQueue_, this->TaskKind_);
         if (alocker.CheckUnlockForInvoke()) {
            ++this->CountImmediate_;
            task();
         }
         else {
            ++this->CountQueued_;
            alocker.AddAsyncTask(task);
         }
      }
      StartingLine_.Wait();
   }
   void PrintInfo(const char* id) {
      std::cout << "RunASAP." << id
         << "|sum=" << (this->CountImmediate_ + this->CountQueued_)
         << "|immediate=" << this->CountImmediate_
         << "|queued=" << this->CountQueued_
         << std::endl;
   }
};
struct RunQueued : public RunAQueueBase {
   fon9_NON_COPY_NON_MOVE(RunQueued);
   std::thread Thread_;
   RunQueued(AQueue& qu, fon9::CyclicBarrier& thrBarrier)
      : RunAQueueBase{qu, thrBarrier}
      , Thread_{&RunQueued::ThrRun, this} {
   }
   ~RunQueued() {
      this->Thread_.join();
   }
   void ThrRun() {
      this->StartingLine_.Wait();
      AQueueTestTask task;
      for (uint64_t L = 0; L < kTimes; ++L)
         this->AQueue_.AddTask(task);
      this->StartingLine_.Wait();
   }
};
fon9_WARN_POP;

void Test_AQueue(unsigned consumerThreadCount) {
   RunCount_ = 0;
   ThreadPool consumer;
   consumer.StartThread(consumerThreadCount, "AQueueTester");

   AQueue               qu{consumer};
   fon9::CyclicBarrier  startingLine(5);
   RunASAP              runASAP1(qu, startingLine, fon9::AQueueTaskKind::Get);
   RunASAP              runASAP2(qu, startingLine, fon9::AQueueTaskKind::Set);
   RunQueued            runQueued1(qu, startingLine);
   RunQueued            runQueued2(qu, startingLine);
   std::cout << "AQueue test: ASAP*2, Queued*2, Consumer*" << consumerThreadCount << std::endl;

   uint64_t          expTimes = (startingLine.ExpectedCount_ - 1) * kTimes;
   fon9::StopWatch   stopWatch;
   startingLine.Wait(); // wait RunASAP/RunQueued thread start.
   // running...
   startingLine.Wait(); // wait RunASAP/RunQueued thread end.
   stopWatch.PrintResult("", expTimes);
   runASAP1.PrintInfo("1");
   runASAP2.PrintInfo("2");

#if 0
   // 如果此時 consumer 還在執行 qu.TakeCall();
   // 且在 consumer.WaitForEndNow(remainHandler); 設定了 ThreadState::EndNow 之後 qu.TakeCall(); 才執行完畢.
   // - 則在 qu.TakeCall() 返回前的  qu.MakeCallForWork(); => consumer.EmplaceMessage() 會失敗!
   // - 造成 qu 還有剩餘的工作未完成.
   // - 因此在最後再執行一次 qu.TakeCall(); 完成所有的測試!
   AQueueHandler remainHandler;
   consumer.WaitForEndNow(remainHandler);
   remainHandler.OnThreadEnd("RemainHandler");
   if (RunCount_ != expTimes) {
      std::cout << "For remaining tasks: qu.TakeCall(): " << (expTimes - RunCount_) << std::endl;
      qu.TakeCall();
   }
#else
   // 為了驗證「一般情況下」可以完成全部的 qu tasks:
   // - 所以使用 sleep 來確保完成全部的 tasks.
   unsigned msWaitCount = 0;
   if (RunCount_ != expTimes) {
      std::cout << "Waiting qu tasks done..." << (expTimes - RunCount_) << std::endl;
      while (RunCount_ != expTimes) {
         std::this_thread::sleep_for(std::chrono::milliseconds{10});
         if (++msWaitCount > 1000)
            break;
         std::atomic_thread_fence(std::memory_order::memory_order_seq_cst);
      }
   }
   consumer.WaitForEndNow();
#endif

   if (RunCount_ != expTimes) {
      std::cout << "[ERROR] RunCount=" << RunCount_ << "|ExpectTimes=" << expTimes << std::endl;
      abort();
   }
   std::cout << "[OK   ] ExpectTimes=" << RunCount_ << std::endl;
}

//--------------------------------------------------------------------------//

int main() {
   fon9::AutoPrintTestInfo utinfo("AQueue");
   std::cout << "sizeof(AQueue)=" << sizeof(AQueue) << std::endl;
   Test_AQueue(1);

   utinfo.PrintSplitter();
   Test_AQueue(2);

   utinfo.PrintSplitter();
   Test_AQueue(4);
}
