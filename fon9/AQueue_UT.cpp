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
   void MakeCallForWork() {
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
fon9_MSC_WARN_DISABLE(4355);// 'this': used in base member initializer list.
struct RunASAP : public RunAQueueBase {
   fon9_NON_COPY_NON_MOVE(RunASAP);
   uint64_t    CountImmediate_{}, CountQueued_{};
   std::thread Thread_;
   RunASAP(AQueue& qu, fon9::CyclicBarrier& thrBarrier)
      : RunAQueueBase{qu, thrBarrier}
      , Thread_{&RunASAP::ThrRun, this} {
   }
   ~RunASAP() {
      this->Thread_.join();
   }
   void ThrRun() {
      this->StartingLine_.Wait();
      AQueueTestTask task;
      for (uint64_t L = 0; L < kTimes; ++L) {
         AQueue::Locker  blocker(this->AQueue_);
         if (blocker.IsAllowInvoke_) {
            ++this->CountImmediate_;
            task();
         }
         else {
            ++this->CountQueued_;
            blocker.AddTask(task);
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
fon9_MSC_WARN_POP;

void Test_AQueue(unsigned consumerThreadCount) {
   RunCount_ = 0;
   ThreadPool consumer;
   consumer.StartThread(consumerThreadCount, "AQueueTester");

   AQueue               qu{consumer};
   fon9::CyclicBarrier  startingLine(5);
   RunASAP              runASAP1(qu, startingLine);
   RunASAP              runASAP2(qu, startingLine);
   RunQueued            runQueued1(qu, startingLine);
   RunQueued            runQueued2(qu, startingLine);
   std::cout << "AQueue test: ASAP*2, Queued*2, Consumer*" << consumerThreadCount << std::endl;

   uint64_t          expTimes = (startingLine.ExpectedCount_ - 1) * kTimes;
   fon9::StopWatch   stopWatch;
   startingLine.Wait(); // wait RunASAP/RunQueued thread start.
   // running...
   startingLine.Wait(); // wait RunASAP/RunQueued thread end.
   stopWatch.PrintResult("", expTimes);

   AQueueHandler remainHandler;
   consumer.WaitForEndNow(remainHandler);
   remainHandler.OnThreadEnd("RemainHandler");

   runASAP1.PrintInfo("1");
   runASAP2.PrintInfo("2");
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
