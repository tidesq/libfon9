// \file fon9/ThreadController_UT.cpp
// \author fonwinz@gmail.com
#include "fon9/Worker.hpp"
#include "fon9/MessageQueue.hpp"
#include "fon9/TestTools.hpp"

//--------------------------------------------------------------------------//

using WorkMessageType = int64_t;
struct WorkContentBase : public fon9::WorkContentBase {
   std::thread::id   LastRunningThreadId_;
   using Messages = std::vector<WorkMessageType>;
   Messages          Messages_;
   WorkMessageType   MessageSum_{0};
   size_t            CountAddWork_{0};
   size_t            CountAddWorkAtWorking_{0};
   size_t            CountNotifyRinging_{0};
   size_t            CountTakeCall_{0};
   size_t            CountMessages_{0};
   size_t            CountRunningThreadChanged_{0};

   bool AddWork(WorkMessageType task) {
      this->Messages_.push_back(task);
      ++this->CountAddWork_;
      if (this->SetToRinging()) {
         ++this->CountNotifyRinging_;
         return true;
      }
      if (this->GetWorkerState() == fon9::WorkerState::Working)
         ++this->CountAddWorkAtWorking_;
      return false;
   }
   fon9::WorkerState TakeCall(std::unique_lock<std::mutex>& lk) {
      ++this->CountTakeCall_;
      std::thread::id curThreadId = std::this_thread::get_id();
      if (this->LastRunningThreadId_ != curThreadId) {
         this->LastRunningThreadId_ = curThreadId;
         ++this->CountRunningThreadChanged_;
      }
      while (!this->Messages_.empty()) {
         WorkContentBase::Messages tasks{std::move(this->Messages_)};
         lk.unlock();
         this->CountMessages_ += tasks.size();
         for (auto v : tasks)
            this->MessageSum_ += v;
         lk.lock();
      }
      return this->GetWorkerState() == fon9::WorkerState::Disposing ? fon9::WorkerState::Disposed : fon9::WorkerState::Sleeping;
   }

   void CheckResult(const char* msg, WorkMessageType messageSum) const {
      std::cout << "Worker test result, " << msg << ":\n"
         << "|CountAddWork=" << this->CountAddWork_
         << "|CountMessages=" << this->CountMessages_
         << "|RemainMessagesSize=" << this->Messages_.size()
         << "\n"
         << "|CountNotifyRinging=" << this->CountNotifyRinging_
         << "|CountAddWorkAtWorking=" << CountAddWorkAtWorking_
         << "|CountTakeCall=" << this->CountTakeCall_
         << "|CountRunningThreadChanged=" << this->CountRunningThreadChanged_
         << "\n"
         << "|MessageSum=" << this->MessageSum_;
      if (this->MessageSum_ != messageSum) {
         std::cout << "|expect.MessageSum=" << messageSum << std::endl;
         abort();
      }
      std::cout << "\n|Test success!" << std::endl;
   }
};
struct WorkContentController : public fon9::MustLock<WorkContentBase> {
   fon9_NON_COPY_NON_MOVE(WorkContentController);
   WorkContentController() = default;

   fon9::WorkerState TakeCall(Locker& lk) {
      return lk->TakeCall(lk);
   }
};

//--------------------------------------------------------------------------//

static const WorkMessageType kWorkTimes = 1000 * 1000;

template <class Worker>
WorkMessageType ExecuteAddTask(Worker& worker) {
   std::array<std::thread, 4>   addTaskThreads;
   std::atomic<WorkMessageType> messageSum{0};
   for (std::thread& thr : addTaskThreads) {
      thr = std::move(std::thread{[&messageSum, &worker]() {
         for (int64_t L = 0; L < kWorkTimes; ++L) {
            worker.AddWork(L);
            messageSum += L;
         }
      }});
   }
   fon9::JoinThreads(addTaskThreads);
   worker.Dispose();
   return messageSum;
}

//--------------------------------------------------------------------------//

void TestWorkerInThreadPool() {
   struct WorkContent : WorkContentController {
      using Waiter = fon9::WaitPolicy_CV;
      Waiter   Waiter_;

      fon9_NON_COPY_NON_MOVE(WorkContent);
      WorkContent() = default;

      void TakeNap(Locker& lk) {
         if (lk->GetWorkerState() < fon9::WorkerState::Disposing)
            this->Waiter_.Wait(lk);
      }
      void Dispose(Locker& lk) {
         if (lk->SetToDisposing())
            this->Waiter_.NotifyAll(lk);
      }
      void AddWork(Locker& lk, WorkMessageType task) {
         if (lk->AddWork(task))
            this->Waiter_.NotifyOne(lk);
      }
   };
   using Worker = fon9::Worker<WorkContent>;
   using ThreadPool = std::array<std::thread, 4>;
   Worker      worker;
   ThreadPool  workThreads;
   for (std::thread& thr : workThreads) {
      thr = std::move(std::thread{[&worker]() {
         while (worker.TakeNap() < fon9::WorkerState::Disposed) {
         }
      }});
   }
   WorkMessageType messageSum = ExecuteAddTask(worker);
   fon9::JoinThreads(workThreads);
   worker.GetWorkContent([&messageSum](Worker::ContentLocker& ctx) {
      ctx->CheckResult("in thread pool", messageSum);
   });
}

//--------------------------------------------------------------------------//

struct WorkerInMessageQueue {
   fon9_NON_COPY_NON_MOVE(WorkerInMessageQueue);

   struct WorkContent;
   using Worker = fon9::Worker<WorkContent>;
   struct WorkerHandler {
      size_t CountOnMessage_{0};
      using MessageType = Worker*;
      using WorkerService = fon9::MessageQueueService<WorkerHandler>;
      WorkerHandler(WorkerService&) {
      }
      void OnThreadEnd(const std::string& thrName) {
         fon9_LOG_ThrRun(thrName, "|CountOnMessage=", this->CountOnMessage_);
      }
      void OnMessage(MessageType worker);
   };
   using WorkerService = WorkerHandler::WorkerService;

   struct WorkContent : WorkContentController {
      fon9_NON_COPY_NON_MOVE(WorkContent);
      WorkerService& WorkerService_;
      WorkContent(WorkerService& workService) : WorkerService_(workService) {
      }
      void Notify(Locker& lk) {
         lk.unlock();
         Worker& worker = Worker::StaticCast(*this);
         this->WorkerService_.EmplaceMessage(&worker);
      }
      void Dispose(Locker& lk) {
         if (lk->SetToDisposing())
            this->Notify(lk);
      }
      void AddWork(Locker& lk, WorkMessageType task) {
         if (lk->AddWork(task))
            this->Notify(lk);
      }
   };
};
void WorkerInMessageQueue::WorkerHandler::OnMessage(MessageType worker) {
   ++this->CountOnMessage_;
   worker->TakeCall();
}

void TestWorkerInMessageQueue() {
   using WorkerService = WorkerInMessageQueue::WorkerService;
   using WorkerHandler = WorkerInMessageQueue::WorkerHandler;
   using Worker        = WorkerInMessageQueue::Worker;

   WorkerService  workerService;
   workerService.StartThread(4, "WorkerService");

   Worker            worker{workerService};
   WorkMessageType   messageSum = ExecuteAddTask(worker);
   workerService.WaitForEndNow();

   size_t remainMessageSize;
   worker.GetWorkContent([&remainMessageSize](Worker::ContentLocker& ctx) {
      remainMessageSize = ctx->Messages_.size();
   });

   WorkerHandler  remainMessageHandler{workerService};
   workerService.WaitForEndNow(remainMessageHandler);

   std::cout << "RemainMessageSize.WhenWorkerServiceDone=" << remainMessageSize << std::endl;
   worker.GetWorkContent([&messageSum](Worker::ContentLocker& ctx) {
      ctx->CheckResult("in WorkerService(MessageQueue)", messageSum);
   });
}

//--------------------------------------------------------------------------//

static std::atomic<int64_t> gHandlerValueSum{0};

class MyMessageHandler {
   uint64_t ValueCount_{0};
   int64_t  ValueSum_{0};
public:
   struct MessageType {
      int Value_;
      MessageType(int v) : Value_{v} {
      }
   };
   using MyQueueService = fon9::MessageQueueService<MyMessageHandler>;

   MyMessageHandler(MyQueueService&) {
   }

   void OnThreadEnd(const std::string& thrName) {
      fon9_LOG_ThrRun("~MessageHandler|name=", thrName, "|valueCount=", this->ValueCount_, "|valueSum=", this->ValueSum_);
      gHandlerValueSum += this->ValueSum_;
   }
   void OnMessage(MessageType v) {
      ++this->ValueCount_;
      this->ValueSum_ += v.Value_;
   }

   static void Test() {
      MyQueueService myQu;
      myQu.StartThread(4, "ThreadControllerTester");
      int64_t addMessageSum = 0;
      for (int L = 0; L < 1000 * 1000; ++L) {
         addMessageSum += L;
         myQu.EmplaceMessage(L);
      }

      MyMessageHandler   remainMessageHandler{myQu};
      myQu.WaitForEndNow(remainMessageHandler);
      remainMessageHandler.OnThreadEnd("After.WaitTerminate.Remain");

      int64_t diff = addMessageSum - gHandlerValueSum;
      fon9_LOG_ThrRun("CheckResult|AddMessageSum=", addMessageSum,
                      "|Thread.ValueSum=", gHandlerValueSum.load(),
                      "|diff=", diff,
                      (diff == 0) ? "|OK" : "|************** ERROR **************");
      if (diff != 0)
         abort();
   }
};

//--------------------------------------------------------------------------//

int main()
{
   fon9::AutoPrintTestInfo utinfo{"ThreadController/MessageQueueService/Worker"};
   MyMessageHandler::Test();

   utinfo.PrintSplitter();
   TestWorkerInThreadPool();

   utinfo.PrintSplitter();
   TestWorkerInMessageQueue();
}
