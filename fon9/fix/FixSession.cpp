// \file fon9/fix/FixSession.cpp
// \author fonwinz@gmail.com
#include "fon9/fix/FixSession.hpp"
#include "fon9/fix/FixRecorder.hpp"
#include "fon9/fix/FixAdminMsg.hpp"

namespace fon9 { namespace fix {

FixSession::FixSession(const FixConfig& cfg)
   : RxArgs_{FixParser_} {
   this->RxArgs_.FixSession_  = this;
   this->RxArgs_.FixReceiver_ = this;
   this->RxArgs_.FixConfig_   = &cfg;
   this->RxArgs_.FixSender_   = nullptr;
}
FixSession::~FixSession() {
}
//--------------------------------------------------------------------------//
void FixSession::ClearFixSession(FixSessionSt st) {
   this->FixSt_ = st;
   this->MsgReceivedCount_ = this->HbTestCount_ = 0;
   this->ClearFeedBuffer();
   this->ClearRecvKeeper();
}
FixSenderSP FixSession::OnFixSessionDisconnected(const StrView& info) {
   this->FixSessionTimerStopNoWait();
   this->ClearFixSession(FixSessionSt::Disconnected);
   if (this->FixSender_) {
      this->FixSender_->GetFixRecorder().Write(f9fix_kCSTR_HdrInfo, info);
      this->RxArgs_.FixSender_ = nullptr;
   }
   return std::move(this->FixSender_);
}
void FixSession::OnFixSessionConnected() {
   this->ClearFixSession(FixSessionSt::Connected);
   this->FixSessionTimerRunAfter(this->RxArgs_.FixConfig_->TiWaitForLogon_);
}
void FixSession::OnFixMessageParsed(StrView fixmsg) {
   ++this->MsgReceivedCount_;
   this->RxArgs_.MsgStr_ = fixmsg;
   if (fon9_LIKELY(this->FixSt_ == FixSessionSt::ApReady)) {
      // 為了加快 Ap 層的處以速度, ApReady 狀態下的訊息, 直接處理.
      this->DispatchFixMessage(this->RxArgs_);
      return;
   }
   switch (this->FixSt_) {
   case FixSessionSt::Disconnected:
      return;
   case FixSessionSt::Connected: // this = Acceptor; fixmsg = Logon Request.
      assert(this->FixSender_.get() == nullptr);
      if (this->Check1stMustLogon())
         this->OnRecvLogonRequest(this->RxArgs_);
      return;
   case FixSessionSt::LogonSent: // this = Initiator; fixmsg = Logon Response.
      if (!this->Check1stMustLogon())
         return;
      break;
   case FixSessionSt::LogonRecovering:
   case FixSessionSt::LogonTest:
   case FixSessionSt::ApReady: // 已在此函式一開始處理過了.
   case FixSessionSt::LogoutPending:
      break;
   }
   this->DispatchFixMessage(this->RxArgs_);
}
bool FixSession::Check1stMustLogon() {
   const FixParser::FixField* fldMsgType = this->FixParser_.GetField(f9fix_kTAG_MsgType);
   if (fldMsgType == nullptr) {
      this->CloseFixSession("Unknown 1st msg.");
      return false;
   }
   static_assert(sizeof(f9fix_kMSGTYPE_Logon) - 1 == 1, "Logon.MsgType error?!");
   if (fon9_LIKELY(fldMsgType->Value_.size() == 1)) {
      const char msgType = *fldMsgType->Value_.begin();
      if (fon9_LIKELY(msgType == *f9fix_kMSGTYPE_Logon))
          return true;
      if (msgType == *f9fix_kMSGTYPE_Logout) {
         std::string errmsg{"1st msg is Logout:"};
         if (const FixParser::FixField* fld = this->FixParser_.GetField(f9fix_kTAG_Text))
            fld->Value_.AppendTo(errmsg);
         this->CloseFixSession(std::move(errmsg));
         return false;
      }
   }
   this->CloseFixSession("1st msg is not Logon.");
   return false;
}
FixParser::Result FixSession::OnFixMessageError(FixParser::Result res, StrView& fixmsgStream, const char* perr) {
   (void)fixmsgStream; (void)perr;
   RevBufferList rbuf{64};
   RevPrint(rbuf, "FixSession.OnFixMessageError:|err=", res);
   this->CloseFixSession(BufferTo<std::string>(rbuf.MoveOut()));
   return res;
}
//--------------------------------------------------------------------------//
static void CopyFixField(RevBuffer& msgbuf, const StrView& fldTo, const FixParser::FixField* fldFrom) {
   if (fldFrom)
      RevPrint(msgbuf, fldTo, fldFrom->Value_);
}
void FixSession::SendLogout(FixBuilder&& fixb, FixRecorder* fixr) {
   if (FixSender* fixout = this->FixSender_.get()) {
      if (this->FixSt_ != FixSessionSt::LogoutPending) {
         this->FixSt_ = FixSessionSt::LogoutPending;
         this->FixSessionTimerRunAfter(this->RxArgs_.FixConfig_->TiLogoutPending_);
         fixout->Send(f9fix_SPLFLDMSGTYPE(Logout), std::move(fixb));
      }
      return;
   }
   this->FixSt_ = FixSessionSt::Disconnected;
   this->FixSessionTimerStopNoWait();
   RevBuffer&   msgbuf = fixb.GetBuffer();
   CopyFixField(msgbuf, f9fix_SPLTAGEQ(TargetCompID), this->FixParser_.GetField(f9fix_kTAG_SenderCompID));
   CopyFixField(msgbuf, f9fix_SPLTAGEQ(TargetSubID),  this->FixParser_.GetField(f9fix_kTAG_SenderSubID));
   CopyFixField(msgbuf, f9fix_SPLTAGEQ(SenderCompID), this->FixParser_.GetField(f9fix_kTAG_TargetCompID));
   CopyFixField(msgbuf, f9fix_SPLTAGEQ(SenderSubID),  this->FixParser_.GetField(f9fix_kTAG_TargetSubID));

   RevPut_TimeFIXMS(msgbuf, UtcNow());
   RevPrint(msgbuf, f9fix_SPLFLDMSGTYPE(Logout) f9fix_SPLTAGEQ(MsgSeqNum) "0" f9fix_SPLTAGEQ(SendingTime));

   BufferList  fixmsg{fixb.Final(ToStrView(this->FixParser_.GetExpectHeader()))};
   std::string cause = BufferTo<std::string>(fixmsg);
   this->OnFixSessionForceSend(std::move(fixmsg));
   if (fixr)
      fixr->Write(f9fix_kCSTR_HdrError, cause);
   StrView  sp{&cause};
   this->FixParser_.Parse(sp);
   if (const FixParser::FixField* fldText = this->FixParser_.GetField(f9fix_kTAG_Text))
      cause = fldText->Value_.ToString("ForceLogout:");
   this->CloseFixSession(std::move(cause));
}
//--------------------------------------------------------------------------//
void FixSession::SendLogon(FixSenderSP fixout, uint32_t heartBtInt, FixBuilder&& fixb) {
   assert(this->FixSt_ < FixSessionSt::LogonSent);
   this->FixSt_ = FixSessionSt::LogonSent;
   this->HeartBtInt_ = heartBtInt;
   if (this->ResetNextSendSeq_ > 0) {
      fixout->ResetNextSendSeq(this->ResetNextSendSeq_);
      this->ResetNextSendSeq_ = 0;
   }
   if (this->ResetNextRecvSeq_ > 0) {
      fixout->GetFixRecorder().ForceResetRecvSeq("Reset RECV seqNum on SendLogon()", this->ResetNextRecvSeq_);
      this->ResetNextRecvSeq_ = 0;
   }
   RevPrint(fixb.GetBuffer(), f9fix_SPLTAGEQ(HeartBtInt), heartBtInt);
   fixout->Send(f9fix_SPLFLDMSGTYPE(Logon), std::move(fixb));
   assert(this->FixSender_.get() == nullptr && (this->RxArgs_.FixSender_ == nullptr || this->RxArgs_.FixSender_ == fixout.get()));
   this->RxArgs_.FixSender_ = fixout.get();
   this->FixSender_ = std::move(fixout);
}
void FixSession::SendLogonResponse(FixSenderSP fixout, uint32_t heartBtInt, FixBuilder&& fixb, const FixRecvEvArgs& rxargs) {
   this->SendLogon(std::move(fixout), heartBtInt, std::move(fixb));
   this->OnLogonResponsed(rxargs, FixReceiver::GapSkipRecord);
}
void FixSession::OnLogonResponsed(const FixRecvEvArgs& rxargs, FixReceiver::GapFlags gapf) {
   if (rxargs.SeqSt_ == FixSeqSt::Conform) {
      // - Logon Request/Response 訊息沒有 gap, 不用回補
      // - 接下來要等對方要求我方補齊(可能會接著收到對方的 ResendRequest)
      this->SendLogonTestRequest();
   }
   else {
      this->FixSt_ = FixSessionSt::LogonRecovering;
      this->FixSessionTimerRunAfter(this->RxArgs_.FixConfig_->TiLogonRecover_);
      this->MsgReceivedCount_ = 0;
      this->OnMsgSeqNumNotExpected(rxargs, gapf);
   }
}
void FixSession::SendLogonTestRequest() {
   if (FixSessionSt::LogonSent <= this->FixSt_ && this->FixSt_ <= FixSessionSt::LogonTest) {
      this->FixSt_ = FixSessionSt::LogonTest;
      this->FixSessionTimerRunAfter(this->RxArgs_.FixConfig_->TiLogonTest_);
      FixBuilder fixb;
      RevPrint(fixb.GetBuffer(), f9fix_SPLTAGEQ(TestReqID) "LogonTest");
      this->FixSender_->Send(f9fix_SPLFLDMSGTYPE(TestRequest), std::move(fixb));
   }
}
void FixSession::SetApReadyStImpl() {
   assert (this->FixSt_ < FixSessionSt::ApReady);
   if (FixSender* fixout = this->FixSender_.get()) {
      fixout->GetFixRecorder().Write(f9fix_kCSTR_HdrInfo, "ApReady");
      this->HbTestCount_ = 0;
      this->FixSt_ = FixSessionSt::ApReady;
      this->FixSessionTimerRunAfter(TimeInterval_Second(this->HeartBtInt_));
      this->OnFixSessionApReady();
   }
   else
      this->CloseFixSession("SetApReadySt:|err=No FixSender.");
}
void FixSession::SendSessionReject(FixSeqNum refSeqNum, FixTag refTagID, const StrView& refMsgType, FixBuilder&& fixb) {
   if (FixSender* fixout = this->FixSender_.get())
      fix::SendSessionReject(*fixout, refSeqNum, refTagID, refMsgType, std::move(fixb));
}
//--------------------------------------------------------------------------//
// 來自 FixReceiver 的事件.
void FixSession::OnRecoverDone(const FixRecvEvArgs& rxargs) {
   baseFixReceiver::OnRecoverDone(rxargs);
   this->SendLogonTestRequest();
}
void FixSession::OnLogoutRequired(const FixRecvEvArgs&, FixBuilder&& fixb) {
   this->SendLogout(std::move(fixb));
}
//--------------------------------------------------------------------------//
void FixSession::FixSessionOnTimer() {
   switch (this->FixSt_) {
   case FixSessionSt::Disconnected:
      break;
   case FixSessionSt::Connected:
   case FixSessionSt::LogonSent:
      this->CloseFixSession("Logon timeout.");
      break;
   case FixSessionSt::LogonRecovering:
      if (this->MsgReceivedCount_ == 0)
         this->CloseFixSession("LogonRecover timeout.");
      else {
         this->MsgReceivedCount_ = 0;
         this->FixSessionTimerRunAfter(this->RxArgs_.FixConfig_->TiLogonRecover_);
      }
      break;
   case FixSessionSt::LogonTest:
      if (this->MsgReceivedCount_ == 0) {
         ++this->HbTestCount_;
         if (this->HbTestCount_ >= this->RxArgs_.FixConfig_->MaxLogonTestCount_) {
            this->CloseFixSession("LogonTest:|err=Remote no response");
            break;
         }
      }
      else {
         this->HbTestCount_ = 0;
         this->MsgReceivedCount_ = 0;
      }
      this->SendLogonTestRequest();
      break;
   case FixSessionSt::LogoutPending:
      this->CloseFixSession("Logout timeout.");
      break;
   case FixSessionSt::ApReady:
      TimeInterval hbTi = UtcNow() - this->FixSender_->GetLastSentTime();
      hbTi = TimeInterval_Second(this->HeartBtInt_) - hbTi;
      if (hbTi > TimeInterval_Second(1)) { // 距離上次送出資料, 還需要再等一會兒, 才到達需要送 Hb 的時間.
         this->FixSessionTimerRunAfter(hbTi);
         break;
      }
      this->FixSessionTimerRunAfter(TimeInterval_Second(this->HeartBtInt_));
      if (this->MsgReceivedCount_ == 0) {
         ++this->HbTestCount_;
         const uint32_t hbTestRequestCount = this->RxArgs_.FixConfig_->HbTestRequestCount_;
         if (this->HbTestCount_ >= hbTestRequestCount + 1) {
            this->CloseFixSession("Remote no response");
            break;
         }
         if (this->HbTestCount_ >= hbTestRequestCount) {
            FixBuilder fixb;
            RevPrint(fixb.GetBuffer(), f9fix_SPLTAGEQ(TestReqID) "AliveTest");
            this->FixSender_->Send(f9fix_SPLFLDMSGTYPE(TestRequest), std::move(fixb));
            break;
         }
      }
      else {
         this->HbTestCount_ = 0;
         this->MsgReceivedCount_ = 0;
      }
      SendHeartbeat(*this->FixSender_, nullptr);
      break;
   }
}
//--------------------------------------------------------------------------//
std::string FixSession::ResetNextSendSeq(StrView strNum) {
   FixSeqNum nextSendSeq = StrTo(StrTrim(&strNum), FixSeqNum{0});
   if (!strNum.empty())
      return "Invalid next SEND seqNum string.";
   std::string res = RevPrintTo<std::string>("New next SEND seqNum = ", nextSendSeq, '\n');
   if (nextSendSeq <= 0) {
      this->ResetNextSendSeq_ = 0;
      return res + "Cleared\n";
   }
   if (this->FixSt_ == FixSessionSt::ApReady) {
      this->ResetNextSendSeq_ = 0;
      assert(this->FixSender_.get() != nullptr);
      this->FixSender_->SequenceReset(nextSendSeq);
      res.append("SequenceReset sent\n");
   }
   else {
      this->ResetNextSendSeq_ = nextSendSeq;
      res.append("Use it when next Logon.\n");
   }
   return res;
}
std::string FixSession::ResetNextRecvSeq(StrView strNum) {
   FixSeqNum nextRecvSeq = StrTo(StrTrim(&strNum), FixSeqNum{0});
   if (!strNum.empty())
      return "Invalid next RECV seqNum string.";
   std::string res = RevPrintTo<std::string>("New next RECV seqNum = ", nextRecvSeq, '\n');
   if (nextRecvSeq <= 0) {
      this->ResetNextRecvSeq_ = 0;
      return res + "Cleared\n";
   }
   if (FixSender* fixout = this->FixSender_.get()) {
      this->ResetNextRecvSeq_ = 0;
      fixout->GetFixRecorder().ForceResetRecvSeq("FixSession.ResetNextRecvSeq", nextRecvSeq);
      res.append("Resetted\n");
   }
   else {
      this->ResetNextRecvSeq_ = nextRecvSeq;
      res.append("Use it when next Logon.\n");
   }
   return res;
}
//--------------------------------------------------------------------------//
void FixSession::InitFixConfig(FixConfig& fixcfg) {
   FixReceiver::InitFixConfig(fixcfg);
   fixcfg.Fetch(f9fix_kMSGTYPE_Heartbeat).FixMsgHandler_ = &FixSession::OnRecvHeartbeat;
   fixcfg.Fetch(f9fix_kMSGTYPE_TestRequest).FixMsgHandler_ = &FixSession::OnRecvTestRequest;

   auto mcfg = &fixcfg.Fetch(f9fix_kMSGTYPE_Logout);
   mcfg->FixSeqAllow_ = FixSeqSt::AllowAnySeq;
   mcfg->FixMsgHandler_ = &FixSession::OnRecvLogout;

   // f9fix_kMSGTYPE_Logon:
   // 這裡只有處理 Logon Response, 並進行回補.
   // Logon Request 在 FixSession::OnFixMessageParsed() 裡面處理.
   mcfg = &fixcfg.Fetch(f9fix_kMSGTYPE_Logon);
   mcfg->FixSeqAllow_ = FixSeqSt::TooHigh;
   mcfg->FixMsgHandler_ = &FixSession::OnRecvLogonResponse;
}
void FixSession::OnRecvHeartbeat(const FixRecvEvArgs& rxargs) {
   rxargs.FixSession_->SetApReadySt();
}
void FixSession::OnRecvTestRequest(const FixRecvEvArgs& rxargs) {
   SendHeartbeat(*rxargs.FixSender_, rxargs.Msg_.GetField(f9fix_kTAG_TestReqID));
   rxargs.FixSession_->SetApReadySt();
}
void FixSession::OnRecvLogonResponse(const FixRecvEvArgs& rxargs) {
   if (rxargs.FixSession_->FixSt_ == FixSessionSt::LogonSent)
      rxargs.FixSession_->OnLogonResponsed(rxargs, FixReceiver::GapDontKeep);
   else
      SendInvalidMsgType(*rxargs.FixSender_, rxargs.Msg_.GetMsgSeqNum(), f9fix_kMSGTYPE_Logon);
}
void FixSession::OnRecvLogout(const FixRecvEvArgs& rxargs) {
   FixSession* fixses = rxargs.FixSession_;
   if (rxargs.SeqSt_ == FixSeqSt::TooLow) {
      fixses->OnMsgSeqNumNotExpected(rxargs, FixReceiver::GapDontKeep);
      return;
   }
   if (rxargs.SeqSt_ != FixSeqSt::Conform)
      rxargs.FixSender_->GetFixRecorder().Write(f9fix_kCSTR_HdrIgnoreRecv, rxargs.MsgStr_);
   if (fixses->FixSt_ == FixSessionSt::ApReady)
      fixses->SendLogout(StrView{"Logout response"});
   if (rxargs.SeqSt_ == FixSeqSt::TooHigh)
      fixses->OnMsgSeqNumNotExpected(rxargs, FixReceiver::GapDontKeep);
}

} } // namespaces
