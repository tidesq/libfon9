// \file fon9/fix/FixSender_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "fon9/TestTools.hpp"
#include "fon9/fix/FixSender.hpp"
#include "fon9/fix/FixAdminDef.hpp"
#include "fon9/fix/FixConfig.hpp"
#include "fon9/fix/FixFeeder.hpp"
#include "fon9/Timer.hpp"
#include "fon9/DefaultThreadPool.hpp"

namespace f9fix = fon9::fix;
#define f9fix_kMSGTYPE_NewOrderSingle  "D"
#define f9fix_kMSGTYPE_ExecutionReport "8"

//--------------------------------------------------------------------------//
void CheckSingleFixMessage(f9fix::FixParser& fixpr, fon9::BufferList&& buf, const char* errmsg) {
   std::string    fixmsgStr = fon9::BufferTo<std::string>(buf);
   fon9::StrView  fixmsg{&fixmsgStr};
   if (fixpr.Parse(fixmsg) <= f9fix::FixParser::NeedsMore || !fixmsg.empty()) {
      std::cout << "|err=" << errmsg;
      std::cout << "\r[ERROR]" << std::endl;
      abort();
   }
}

void TestFixSenderWrite1(f9fix::FixSender& fixSender, unsigned count, const fon9::StrView& fldMsgType, fon9::StrView apFields) {
   for (unsigned L = 0; L < count; ++L) {
      f9fix::FixBuilder fixb;
      RevPrint(fixb.GetBuffer(), apFields);
      fixSender.Send(fldMsgType, std::move(fixb));
   }
}
void TestFixSenderWrite2(f9fix::FixSender& fixSender, unsigned count, const fon9::StrView& fldMsgType, fon9::StrView apFields) {
   f9fix::FixParser fixpr;
   for (unsigned L = 0; L < count; ++L) {
      f9fix::FixBuilder    fixb;
      fon9::RevBufferList  fixmsgDupOut{128};
      RevPrint(fixb.GetBuffer(), apFields);
      fixSender.Send(fldMsgType, std::move(fixb), &fixmsgDupOut);
      CheckSingleFixMessage(fixpr, fixmsgDupOut.MoveOut(), "fixmsg dup out error.");
   }
}

//--------------------------------------------------------------------------//

fon9_WARN_DISABLE_PADDING;
int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

   fon9::AutoPrintTestInfo utinfo{"FixSender"};
   fon9::GetDefaultTimerThread();
   fon9::GetDefaultThreadPool();
   std::this_thread::sleep_for(std::chrono::milliseconds{10});

   const char  fixrFileName[] = "FixSender_UT.log";
   remove(fixrFileName);

   struct FixSender : public f9fix::FixSender {
      fon9_NON_COPY_NON_MOVE(FixSender);
      using base = f9fix::FixSender;
      f9fix::FixFeeder* FixFeeder_{nullptr};
      f9fix::FixParser  FixParser_;
      f9fix::FixSeqNum  ExpectedSendSeqNum_{0};
      void OnSendFixMessage(const Locker&, fon9::BufferList buf) override {
         if (this->FixFeeder_) {
            fon9::DcQueueList dcq{std::move(buf)};
            auto res = this->FixFeeder_->FeedBuffer(dcq);
            if (res <= f9fix::FixParser::NeedsMore || dcq.CalcSize() > 0) {
               std::cout << "|err=OnSendFixMessage()|err=" << res
                  << "|remainSize=" << dcq.CalcSize()
                  << "\r[ERROR]" << std::endl;
               abort();
            }
            return;
         }
         CheckSingleFixMessage(this->FixParser_, std::move(buf), "FixSender.OnSendFixMessage()");
         if (++this->ExpectedSendSeqNum_ != this->FixParser_.GetMsgSeqNum()) {
            std::cout << "|err=Unexpected SendSeqNum|expected=" << this->ExpectedSendSeqNum_
               << "|current=" << this->FixParser_.GetMsgSeqNum()
               << "\r[ERROR]" << std::endl;
            abort();
         }
      }
      using base::base;
      using base::Initialize;
      f9fix::FixSeqNum GetNextSendSeq() {
         return this->base::GetNextSendSeq(this->Lock());
      }
   };
   f9fix::CompIDs       compIds{"SenderCoId", "SenderSubId", "TargetCoId", "TargetSubId"};
   f9fix::FixSenderSP   fixSender{new FixSender(f9fix_BEGIN_HEADER_V44, f9fix::CompIDs{compIds})};
   auto res = static_cast<FixSender*>(fixSender.get())->Initialize(fixrFileName);
   if (!res) {
      std::cout << "Open FixRecorder|fileName=" << fixrFileName
         << "|err=" << fon9::RevPrintTo<std::string>(res) << std::endl;
      abort();
   }

   std::cout << "[TEST ] Build(Send) test messages.";
   TestFixSenderWrite1(*fixSender, 100, f9fix_SPLFLDMSGTYPE(NewOrderSingle),  f9fix_SPLTAGEQ(Text) "NewOrderSingle");
   TestFixSenderWrite1(*fixSender,   1, f9fix_SPLFLDMSGTYPE(Heartbeat),       f9fix_SPLTAGEQ(Text) "Heartbeat");
   TestFixSenderWrite1(*fixSender, 100, f9fix_SPLFLDMSGTYPE(ExecutionReport), f9fix_SPLTAGEQ(Text) "ExecutionReport");
   TestFixSenderWrite1(*fixSender,   1, f9fix_SPLFLDMSGTYPE(Heartbeat),       f9fix_SPLTAGEQ(Text) "Heartbeat1");

   TestFixSenderWrite2(*fixSender, 100, f9fix_SPLFLDMSGTYPE(NewOrderSingle),  f9fix_SPLTAGEQ(Text) "NewOrderSingle2");
   TestFixSenderWrite2(*fixSender,   1, f9fix_SPLFLDMSGTYPE(Heartbeat),       f9fix_SPLTAGEQ(Text) "Heartbeat2");
   TestFixSenderWrite2(*fixSender, 100, f9fix_SPLFLDMSGTYPE(ExecutionReport), f9fix_SPLTAGEQ(Text) "ExecutionReport2");
   std::cout << "\r[OK   ]\n";

   struct FixFeeder : public f9fix::FixFeeder {
      fon9_NON_COPY_NON_MOVE(FixFeeder);
      FixFeeder() = default;
      unsigned          Count_;
      f9fix::FixSeqNum  ExpectedSeqNum_;
      void OnFixMessageParsed(fon9::StrView fixmsg) override {
         (void)fixmsg;
         ++this->Count_;
         if (this->FixParser_.GetMsgSeqNum() != this->ExpectedSeqNum_) {
            std::cout << "|err=Unexpected Resend SeqNum|expected=" << this->ExpectedSeqNum_
               << "|current=" << this->FixParser_.GetMsgSeqNum()
               << "\r[ERROR]" << std::endl;
            abort();
         }
         ++this->ExpectedSeqNum_;
         if (auto fldMsgType = this->FixParser_.GetField(f9fix_kTAG_MsgType)) {
            if(fldMsgType->Value_ == f9fix_kMSGTYPE_SequenceReset) {
               if (auto fldNewSeqNo = this->FixParser_.GetField(f9fix_kTAG_NewSeqNo))
                  this->ExpectedSeqNum_ = fon9::StrTo(fldNewSeqNo->Value_, this->ExpectedSeqNum_);
            }
            return;
         }
         std::cout << "|errResend unknown MsgType"
            << "\r[ERROR]" << std::endl;
         abort();
      }
      void CheckReplayDone(f9fix::FixSeqNum nextSeqNum) {
         if (nextSeqNum != this->ExpectedSeqNum_) {
            std::cout << "|err=After replay unexpected SeqNum|expected=" << nextSeqNum
               << "|current=" << this->ExpectedSeqNum_
               << "\r[ERROR]" << std::endl;
            abort();
         }
      }
   };

   auto nextSendSeq = static_cast<FixSender*>(fixSender.get())->GetNextSendSeq();
   std::cout << "[TEST ] Replay 1..0";
   FixFeeder                fixFeeder;
   f9fix::FixConfig         fixConfig;
   f9fix::FixMsgTypeConfig* msgcfg;
   msgcfg = &fixConfig.Fetch(f9fix_kMSGTYPE_ExecutionReport);
   msgcfg->SetInfiniteTTL();
   msgcfg->FixSeqAllow_ = f9fix::FixSeqSt::Conform;
   msgcfg = &fixConfig.Fetch(f9fix_kMSGTYPE_NewOrderSingle);
   msgcfg->FixSeqAllow_ = f9fix::FixSeqSt::Conform;
   msgcfg = &fixConfig.Fetch(f9fix_kMSGTYPE_Heartbeat);
   msgcfg->FixSeqAllow_ = f9fix::FixSeqSt::Conform;
   static_cast<FixSender*>(fixSender.get())->FixFeeder_ = &fixFeeder;
   fixFeeder.Count_ = 0;
   fixSender->Replay(fixConfig, fixFeeder.ExpectedSeqNum_ = 1, 0);
   fixFeeder.CheckReplayDone(nextSendSeq);
   std::cout << "|count(Include SeqReset)=" << fixFeeder.Count_ << "\r[OK   ]\n";

   std::cout << "[TEST ] Hb, Replay 1..0";
   TestFixSenderWrite2(*fixSender, 1, f9fix_SPLFLDMSGTYPE(Heartbeat), f9fix_SPLTAGEQ(Text) "Heartbeat3");
   fixFeeder.Count_ = 0;
   fixSender->Replay(fixConfig, fixFeeder.ExpectedSeqNum_ = 1, 0);
   fixFeeder.CheckReplayDone(++nextSendSeq);
   std::cout << "|count(Include SeqReset)=" << fixFeeder.Count_ << "\r[OK   ]\n";

   std::cout << "[TEST ] Replay 50..150";
   fixFeeder.Count_ = 0;
   fixSender->Replay(fixConfig, fixFeeder.ExpectedSeqNum_ = 50, 150);
   fixFeeder.CheckReplayDone(151);
   std::cout << "|count(Include SeqReset)=" << fixFeeder.Count_ << "\r[OK   ]\n";

   std::cout << "[TEST ] GapFill 50..150";
   fixFeeder.Count_ = 0;
   fixSender->GapFill(fixFeeder.ExpectedSeqNum_ = 50, 150);
   fixFeeder.CheckReplayDone(151);
   std::cout << "|count(Include SeqReset)=" << fixFeeder.Count_ << "\r[OK   ]\n";

   std::cout << "[TEST ] GapFill 50..0";
   fixFeeder.Count_ = 0;
   fixSender->GapFill(fixFeeder.ExpectedSeqNum_ = 50, 0);
   fixFeeder.CheckReplayDone(nextSendSeq);
   std::cout << "|count(Include SeqReset)=" << fixFeeder.Count_ << "\r[OK   ]\n";

   // 結束前刪除測試檔.
   fixSender.reset();
   if (!fon9::IsKeepTestFiles(argc, argv))
      fon9::WaitRemoveFile(fixrFileName);
}
fon9_WARN_POP;
