// \file fon9/fix/FixReceiver_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "fon9/TestTools.hpp"
#include "fon9/fix/FixReceiver.hpp"
#include "fon9/fix/FixAdminMsg.hpp"
#include "fon9/Timer.hpp"
#include "fon9/DefaultThreadPool.hpp"

namespace f9fix = fon9::fix;

int main(int argc, char** argv) {
   #if defined(_MSC_VER) && defined(_DEBUG)
      _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
      //_CrtSetBreakAlloc(176);
   #endif
   fon9::AutoPrintTestInfo utinfo{"FixReceiver"};
   if (argc < 2) {
      std::cout << "Usage: log [input [output]]\n";
      return 3;
   }
   FILE* infd;
   if (argc < 3)
      infd = stdin;
   else if ((infd = fopen(argv[2], "rt")) == nullptr) {
      perror("Open input file error:");
      return 3;
   }
   FILE* outfd;
   if (argc < 4)
      outfd = stdout;
   else if ((outfd = fopen(argv[3], "wt")) == nullptr) {
      perror("Open output file error:");
      return 3;
   }

   fon9::GetDefaultTimerThread();
   fon9::GetDefaultThreadPool();
   std::this_thread::sleep_for(std::chrono::milliseconds{10});

   #define f9fix_kMSGTYPE_ExecutionReport    "8"
   #define f9fix_kMSGTYPE_NewOrderSingle     "D"
   f9fix::FixConfig fixc;
   f9fix::FixReceiver::InitFixConfig(fixc);
   fixc.Fetch(f9fix_kMSGTYPE_ExecutionReport).SetInfiniteTTL();
   fixc.Fetch(f9fix_kMSGTYPE_ExecutionReport).FixMsgHandler_ = [](const f9fix::FixRecvEvArgs&) {};
   fixc.Fetch(f9fix_kMSGTYPE_NewOrderSingle).TTL_ = fon9::TimeInterval_Second(3);
   fixc.Fetch(f9fix_kMSGTYPE_NewOrderSingle).FixMsgHandler_ = [](const f9fix::FixRecvEvArgs&) {};
   fixc.Fetch(f9fix_kMSGTYPE_TestRequest).FixMsgHandler_ = [](const f9fix::FixRecvEvArgs& args) {
      f9fix::SendHeartbeat(*args.FixSender_, args.Msg_.GetField(f9fix_kTAG_TestReqID));
   };

   struct FixTestOutput : public f9fix::FixSender {
      fon9_NON_COPY_NON_MOVE(FixTestOutput);
      FILE* outfd;
      using f9fix::FixSender::FixSender;
      void OnSendFixMessage(Locker&, fon9::BufferList buf) override {
         fprintf(outfd, "FIX-OUT:%s\n", fon9::BufferTo<std::string>(buf).c_str());
      }
   };
   f9fix::FixSenderSP fixo{new FixTestOutput{f9fix_BEGIN_HEADER_V42,
                                             f9fix::CompIDs{"TargetId", "TargetSubId", "SenderId", "SenderSubId"}}};

   auto  rres = fixo->GetFixRecorder().Initialize(argv[1]);
   if (!rres) {
      std::cout << "FixRecorder.Initialize:|err=" << fon9::RevPrintTo<std::string>(rres) << std::endl;
      return 3;
   }
   static_cast<FixTestOutput*>(fixo.get())->outfd = outfd;

   char                 strbuf[f9fix::FixRecorder::kMaxFixMsgBufferSize];
   f9fix::FixReceiver   fixr;
   f9fix::FixParser     fixParser;
   f9fix::FixRecvEvArgs recvArgs{fixParser};
   recvArgs.FixSession_  = nullptr;
   recvArgs.FixSender_   = fixo.get();
   recvArgs.FixReceiver_ = &fixr;
   recvArgs.FixConfig_   = &fixc;

   while (fgets(strbuf, sizeof(strbuf), infd)) {
      recvArgs.MsgStr_ = fon9::StrView_cstr(strbuf);
      fon9::StrTrim(&recvArgs.MsgStr_);
      if (recvArgs.MsgStr_.empty()
       || recvArgs.MsgStr_.Get1st() == '#')
         continue;
      fon9::StrView  fixmsg{recvArgs.MsgStr_};
      auto           pres = fixParser.Parse(fixmsg);
      if (pres > f9fix::FixParser::NeedsMore)
         fixr.Receive(recvArgs);
      else
         fixo->GetFixRecorder().Write(f9fix_kCSTR_HdrError, recvArgs.MsgStr_, "\n"
                                      f9fix_kCSTR_HdrError "ParseError:|err=", pres);
   }
   return 0;
}
