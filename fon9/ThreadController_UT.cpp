// \file fon9/ThreadController_UT.cpp
// \author fonwinz@gmail.com
#include "fon9/MessageQueue.hpp"
#include "fon9/TestTools.hpp"

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
   using MyQueueService = fon9::MessageQueueService<MyMessageHandler>;

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
   myQu.StartThread(4, "ThreadControllerTester");
   int64_t addMessageSum = 0;
   for (int L = 0; L < 1000 * 1000; ++L) {
      addMessageSum += L;
      myQu.EmplaceMessage(L);
   }

   MyMessageHandler   remainMessageHandler{myQu};
   myQu.WaitForEndNow(remainMessageHandler);
   remainMessageHandler.OnThreadEnd("After.WaitTerminate.Remain");

   int64_t diff = addMessageSum - handlerValueSum;
   fon9_LOG_ThrRun("CheckResult|AddMessageSum=", addMessageSum,
                   "|Thread.ValueSum=", handlerValueSum.load(),
                   "|diff=", diff,
                   (diff == 0) ? "|OK" : "|************** ERROR **************");
   if (diff != 0)
      abort();
}
