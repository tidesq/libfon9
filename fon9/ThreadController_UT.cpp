// \file fon9/ThreadController_UT.cpp
// \author fonwinz@gmail.com
#include "fon9/ThreadController.hpp"
#include "fon9/buffer/RevPrint.hpp"
#include "fon9/TestTools.hpp"
#include <queue>

#ifndef fon9_LOG_ThrRun
template <class... ArgsT>
void fon9_LOG_ThrRun(ArgsT&&... args) {
   fon9::RevBufferFixedSize<1024> rbuf;
   RevPrint(rbuf, std::forward<ArgsT>(args)..., "\n\0");
   std::cout << rbuf.GetCurrent();
}
#endif

/// - 每個 thread 會建立一個 MessageHandlerT.
/// - MessageHandlerT 必須提供:
///   - typename MessageHandlerT::MessageType;
///   - MessageHandlerT::MessageHandlerT(MessageQueueService&);
///   - void MessageHandlerT::OnMessage(MessageT&);
///   - void MessageHandlerT::OnThreadEnd(const std::string& thrName);
template <class MessageHandlerT,
   class MessageT = typename MessageHandlerT::MessageType,
   class MessageQueueT = std::queue<MessageT>,
   class WaitPolicy = fon9::WaitPolicy_CV>
class MessageQueueService {
   fon9_NON_COPY_NON_MOVE(MessageQueueService);
   using QueueController = fon9::ThreadController<MessageQueueT, WaitPolicy>;

   static void ThrRun(std::string thrName, MessageQueueService* pthis) {
      fon9_LOG_ThrRun("MessageQueueService.ThrRun|name=", thrName);
      QueueController&  queueController = pthis->QueueController_;
      MessageHandlerT   messageHandler(*pthis);
      for (;;) {
         typename QueueController::Locker  queue{queueController};
         if (!queueController.Wait(queue))
            break;
         while (!queue->empty()) {
            {  // for value auto dtor.
               MessageT value(std::move(queue->front()));
               queue->pop();
               queue.unlock();
               messageHandler.OnMessage(value);
            }
            queue.lock();
            if (!queueController.CheckRunning(queue))
               goto __END_ThrRun;
         }
      }
      __END_ThrRun:
      fon9_LOG_ThrRun("MessageQueueService.ThrRun.End|name=", thrName);
      messageHandler.OnThreadEnd(thrName);
   }

   using ThreadPool = std::vector<std::thread>;
   QueueController   QueueController_;
   ThreadPool        ThreadPool_;
public:
   using MessageHandler = MessageHandlerT;
   using MessageType = MessageT;

   MessageQueueService() {
   }
   ~MessageQueueService() {
      this->WaitTerminate(nullptr);
   }

   void StartThread(uint32_t threadCount) {
      this->QueueController_.OnBeforeThreadStart(threadCount);
      fon9::RevBufferFixedSize<1024> rbuf;
      for (unsigned id = 0; id < threadCount;) {
         rbuf.Rewind();
         fon9::RevPrint(rbuf, "MessageQueueService.", ++id);
         this->ThreadPool_.emplace_back(&ThrRun,
                                        std::string{rbuf.GetCurrent(), rbuf.GetMemEnd()},
                                        this);
      }
   }

   void WaitTerminate(MessageHandlerT* remainMessageHandler) {
      this->QueueController_.WaitTerminate();
      for (std::thread& thr : this->ThreadPool_) {
         if (thr.joinable())
            thr.join();
      }
      if (remainMessageHandler) {
         typename QueueController::Locker  queue{this->QueueController_};
         while (!queue->empty()) {
            remainMessageHandler->OnMessage(queue->front());
            queue->pop();
         }
      }
   }

   template <class... ArgsT>
   void AddMessage(ArgsT&&... args) {
      typename QueueController::Locker queue{this->QueueController_};
      queue->emplace(std::forward<ArgsT>(args)...);
      this->QueueController_.NotifyOne(queue);
   }
};

//--------------------------------------------------------------------------//

static std::atomic<int64_t> handlerValueSum{0};

class MyMessageHandler {
   uint64_t ValueCount_{0};
   int64_t  ValueSum_{0};
public:
   struct MessageType {
      int Value_;
      MessageType(int v) : Value_{v} {
      }
   };
   using MyQueueService = MessageQueueService<MyMessageHandler>;

   MyMessageHandler(MyQueueService&) {
   }

   void OnThreadEnd(const std::string& thrName) {
      fon9_LOG_ThrRun("~MessageHandler|name=", thrName, "|valueCount=", this->ValueCount_, "|valueSum=", this->ValueSum_);
      handlerValueSum += this->ValueSum_;
   }
   void OnMessage(MessageType v) {
      ++this->ValueCount_;
      this->ValueSum_ += v.Value_;
   }
};

//--------------------------------------------------------------------------//

int main()
{
   fon9::AutoPrintTestInfo utinfo{"ThreadController"};

   MyMessageHandler::MyQueueService myQu;
   myQu.StartThread(4);
   int64_t addMessageSum = 0;
   for (int L = 0; L < 1000 * 1000; ++L) {
      addMessageSum += L;
      myQu.AddMessage(L);
   }

   MyMessageHandler remainMessageHandler{myQu};
   myQu.WaitTerminate(&remainMessageHandler);
   remainMessageHandler.OnThreadEnd("After.WaitTerminate.Remain");

   int64_t diff = addMessageSum - handlerValueSum;
   fon9_LOG_ThrRun("CheckResult|AddMessageSum=", addMessageSum,
                   "|Thread.ValueSum=", handlerValueSum.load(),
                   "|diff=", diff,
                   (diff == 0) ? "|OK" : "|************** ERROR **************");
   if (diff != 0)
      abort();
}
