// \file fon9/fix/IoFixSession.cpp
// \author fonwinz@gmail.com
#include "fon9/fix/IoFixSession.hpp"
#include "fon9/fix/FixAdminMsg.hpp"

namespace fon9 { namespace fix {

IoFixManager::~IoFixManager() {
}
void IoFixManager::OnLogonInitiate(IoFixSession& fixses, uint32_t heartBtInt, FixBuilder&& fixb, FixSenderSP fixout) {
   fixout->GetFixRecorder().Write(f9fix_kCSTR_HdrInfo, "OnLogon.Initiate:|from=", fixses.GetDeviceId());
   fixses.SendLogon(std::move(fixout), heartBtInt, std::move(fixb));
}
bool IoFixManager::OnLogonAccepted(FixRecvEvArgs& rxargs, FixSenderSP fixout) {
   assert(rxargs.FixSender_ == nullptr);
   rxargs.FixSender_ = fixout.get();
   rxargs.ResetSeqSt();

   FixRecorder& recorder = fixout->GetFixRecorder();
   recorder.Write(f9fix_kCSTR_HdrInfo, "OnLogon.Accepted:|from=", static_cast<IoFixSession*>(rxargs.FixSession_)->GetDeviceId());
   FixBuilder  fixb;
   if (rxargs.SeqSt_ == FixSeqSt::TooLow) {
      recorder.Write(f9fix_kCSTR_HdrError, rxargs.MsgStr_);
      RevPrint(fixb.GetBuffer(), f9fix_SPLTAGEQ(Text)
               "Bad Logon, MsgSeqNum too low, expecting ", recorder.GetNextRecvSeq(),
               " but received ", rxargs.Msg_.GetMsgSeqNum());
      rxargs.FixSession_->SendLogout(std::move(fixb), &recorder);
      return false;
   }
   FixTag errTag;
   if (const FixParser::FixField* fldHbInt = rxargs.Msg_.GetField(errTag = f9fix_kTAG_HeartBtInt)) {
      if (uint32_t hbInt = StrTo(fldHbInt->Value_, static_cast<uint32_t>(0))) { // hbInt 必須 > 0
         if (const FixParser::FixField* fldEncryptMethod = rxargs.Msg_.GetField(errTag = f9fix_kTAG_EncryptMethod)) {
            if (fldEncryptMethod->Value_.Get1st() == *f9fix_kCSTR_EncryptMethod_None) { // EncryptMethod 必須為 None
               if (rxargs.SeqSt_ == FixSeqSt::Conform)
                  recorder.WriteInputConform(rxargs.MsgStr_);
               else
                  recorder.Write(f9fix_kCSTR_HdrIgnoreRecv, rxargs.MsgStr_);
               RevPrint(fixb.GetBuffer(), f9fix_kFLD_EncryptMethod_None);
               rxargs.FixSession_->SendLogonResponse(fixout, hbInt, std::move(fixb), rxargs);
               return true;
            }
         }
      }
   }
   recorder.Write(f9fix_kCSTR_HdrError, rxargs.MsgStr_);
   RevPrint(fixb.GetBuffer(), f9fix_SPLTAGEQ(Text) "Bad Logon, Tag#", errTag);
   rxargs.FixSession_->SendLogout(std::move(fixb), &recorder);
   return false;
}
//--------------------------------------------------------------------------//
void IoFixSession::OnRecvLogonRequest(FixRecvEvArgs& rxargs) {
   this->FixManager_.OnRecvLogonRequest(rxargs);
}
void IoFixSession::OnFixSessionApReady() {
   this->FixManager_.OnFixSessionApReady(*this);
}
void IoFixSession::OnFixSessionForceSend(BufferList&& msg) {
   assert(this->Dev_);
   this->Dev_->Send(std::move(msg));
}
//--------------------------------------------------------------------------//
void IoFixSession::CloseFixSession(std::string cause) {
   if (this->Dev_)
      this->Dev_->AsyncLingerClose(std::move(cause));
}
void IoFixSession::FixSessionTimerStopNoWait() {
   // 使用 this->Dev_->CommonTimerRunAfter(after); 不需要額外的 Timer Stop.
}
void IoFixSession::FixSessionTimerRunAfter(TimeInterval after) {
   if (this->Dev_)
      this->Dev_->CommonTimerRunAfter(after);
}
void IoFixSession::OnDevice_CommonTimer(io::Device& dev, TimeStamp now) {
   (void)now;
   dev.OpQueue_.AddTask(io::DeviceAsyncOp{std::bind(&IoFixSession::FixSessionOnTimer, IoFixSessionSP{this})});
}
//--------------------------------------------------------------------------//
void IoFixSession::OnDevice_Initialized(io::Device& dev) {
   assert(this->Dev_ == nullptr);
   this->Dev_ = &dev;
}
void IoFixSession::OnDevice_Destructing(io::Device&) {
   this->Dev_ = nullptr;
}
void IoFixSession::OnDevice_StateChanged(io::Device&, const io::StateChangedArgs& e) {
   if (e.BeforeState_ == io::State::LinkReady) {
      if (auto fixout = this->OnFixSessionDisconnected(e.After_.Info_))
         this->FixManager_.OnFixSessionDisconnected(*this, std::move(fixout));
   }
}
io::RecvBufferSize IoFixSession::OnDevice_LinkReady(io::Device&) {
   this->OnFixSessionConnected();
   this->FixManager_.OnFixSessionConnected(*this);
   return static_cast<io::RecvBufferSize>(FixRecorder::kMaxFixMsgBufferSize);
}
std::string IoFixSession::SessionCommand(io::Device& dev, const StrView cmdln) {
   StrView  param{cmdln};
   StrView  cmd = StrFetchTrim(param, &isspace);
   if (cmd == "?") {
      static const char cstrHelp[] =
         "snext"   fon9_kCSTR_CELLSPL "Set next SEND FixSeqNum"  fon9_kCSTR_CELLSPL "New next SEND seqNum." fon9_kCSTR_ROWSPL
         "rnext"   fon9_kCSTR_CELLSPL "Set next RECV FixSeqNum"  fon9_kCSTR_CELLSPL "New next RECV seqNum." fon9_kCSTR_ROWSPL;
      return cstrHelp;
   }
   else if (cmd == "snext") {
      return this->ResetNextSendSeq(param);
   }
   else if (cmd == "rnext") {
      return this->ResetNextRecvSeq(param);
   }
   return baseIo::SessionCommand(dev, cmdln);
}
io::RecvBufferSize IoFixSession::OnDevice_Recv(io::Device&, DcQueueList& rxbuf) {
   return this->FeedBuffer(rxbuf) < FixParser::NeedsMore
      ? io::RecvBufferSize::NoRecvEvent
      : static_cast<io::RecvBufferSize>(FixRecorder::kMaxFixMsgBufferSize);
}

} } // namespaces
