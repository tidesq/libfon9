// \file fon9/fix/FixReceiver.cpp
// \author fonwinz@gmail.com
#include "fon9/fix/FixReceiver.hpp"
#include "fon9/fix/FixAdminMsg.hpp"

namespace fon9 { namespace fix {
   
FixReceiver::~FixReceiver() {
}
void FixReceiver::ClearRecvKeeper() {
   this->MsgKeeper_.clear();
   this->ResendRequestEndSeqNo_ = this->LastGapSeqNo_ = 0;
}
//--------------------------------------------------------------------------//
void FixReceiver::OnRecvPossDup(const FixRecvEvArgs& rxargs) {
   (void)rxargs;
}
void FixReceiver::OnLogoutRequired(const FixRecvEvArgs& rxargs, FixBuilder&& fixb) {
   rxargs.FixSender_->Send(f9fix_SPLFLDMSGTYPE(Logout), std::move(fixb));
}
//--------------------------------------------------------------------------//
FixTag FixReceiver::CheckCompID(const FixRecvEvArgs& rxargs, const StrView& msgType) {
   FixTag errTag = rxargs.FixSender_->CompIDs_.Check(rxargs.Msg_);
   if (errTag)
      this->OnBadCompID(rxargs, errTag, msgType);
   return errTag;
}
void FixReceiver::OnBadCompID(const FixRecvEvArgs& rxargs, FixTag errTag, const StrView& msgType) {
   FixRecorder&   fixRecorder = rxargs.FixSender_->GetFixRecorder();
   // 為了避免對方系統出錯, 如果「只寫 err 序號不變」,
   // 則 ResendRequest 可能再次補到 CompID 有問題的訊息,
   // 造成訊息無法繼續下去.
   // 所以如果此筆的序號正確, 則使用 WriteInputConform(), 讓下一筆正確序號的訊息能進入系統.
   if (rxargs.SeqSt_ == FixSeqSt::Conform)
      fixRecorder.WriteInputConform(rxargs.MsgStr_);
   else
      fixRecorder.Write(f9fix_kCSTR_HdrError, rxargs.MsgStr_);
   FixBuilder  fixb;
   RevPrint(fixb.GetBuffer(), f9fix_kFLD_SessionRejectReason_CompIDProblem f9fix_SPLTAGEQ(Text) "CompID problem");
   SendSessionReject(*rxargs.FixSender_, rxargs.Msg_.GetMsgSeqNum(), errTag, msgType, std::move(fixb));
}
//--------------------------------------------------------------------------//
void FixReceiver::InitFixConfig(FixConfig& fixcfg) {
   FixMsgTypeConfig* mcfg = &fixcfg.Fetch(f9fix_kMSGTYPE_SequenceReset);
   mcfg->FixSeqAllow_ = FixSeqSt::AllowAnySeq | FixSeqSt::NoPreRecord;
   mcfg->FixMsgHandler_ = &FixReceiver::OnRecvSequenceReset;

   mcfg = &fixcfg.Fetch(f9fix_kMSGTYPE_ResendRequest);
   mcfg->FixSeqAllow_ = FixSeqSt::AllowAnySeq | FixSeqSt::NoPreRecord;
   mcfg->FixMsgHandler_ = &OnRecvResendRequest;
}
void FixReceiver::OnRecvResendRequest(const FixRecvEvArgs& rxargs) {
   if (rxargs.SeqSt_ != FixSeqSt::Conform)
      rxargs.FixSender_->GetFixRecorder().Write(f9fix_kCSTR_HdrReplay, rxargs.MsgStr_);
   FixTag errTag;
   if (const FixParser::FixField* fldBeginSeqNo = rxargs.Msg_.GetField(errTag = f9fix_kTAG_BeginSeqNo)) {
      if (const FixParser::FixField* fldEndSeqNo = rxargs.Msg_.GetField(errTag = f9fix_kTAG_EndSeqNo)) {
         FixSeqNum beginSeqNo = StrTo(fldBeginSeqNo->Value_, FixSeqNum{});
         FixSeqNum endSeqNo = StrTo(fldEndSeqNo->Value_, FixSeqNum{});
         rxargs.FixSender_->Replay(*rxargs.FixConfig_, beginSeqNo, endSeqNo);
         if (rxargs.SeqSt_ != FixSeqSt::Conform)
            rxargs.FixReceiver_->OnMsgSeqNumNotExpected(rxargs, FixReceiver::GapSkipRecord);
         return;
      }
   }
   SendRequiredTagMissing(*rxargs.FixSender_, rxargs.Msg_.GetMsgSeqNum(), errTag, f9fix_kMSGTYPE_ResendRequest);
   if (rxargs.SeqSt_ != FixSeqSt::Conform)
      rxargs.FixReceiver_->OnMsgSeqNumNotExpected(rxargs, FixReceiver::GapSkipRecord);
}
void FixReceiver::OnRecvSequenceReset(const FixRecvEvArgs& rxargs) {
   bool isGapFill = false;
   if (const FixParser::FixField* fldGapFill = rxargs.Msg_.GetField(f9fix_kTAG_GapFillFlag))
      isGapFill = (fldGapFill->Value_.Get1st() == 'Y');

   if (isGapFill && rxargs.SeqSt_ != FixSeqSt::Conform) {
      // GapFill 必須是正確序號(msgSeqNum).
      rxargs.FixReceiver_->OnMsgSeqNumNotExpected(rxargs, GapDontKeep);
      return;
   }

   FixRecorder&     fixRecorder = rxargs.FixSender_->GetFixRecorder();
   const FixSeqNum  msgSeqNum = rxargs.Msg_.GetMsgSeqNum();
   const FixParser::FixField* fldNewSeqNo = rxargs.Msg_.GetField(f9fix_kTAG_NewSeqNo);
   if (fldNewSeqNo == nullptr) {
      fixRecorder.Write(f9fix_kCSTR_HdrError, rxargs.MsgStr_);
      SendRequiredTagMissing(*rxargs.FixSender_, msgSeqNum, f9fix_kTAG_NewSeqNo, f9fix_kMSGTYPE_SequenceReset);
      return;
   }
   FixSeqNum newSeqNo = StrTo(fldNewSeqNo->Value_, FixSeqNum{0});

   if (isGapFill) {
      if (fon9_UNLIKELY(newSeqNo < msgSeqNum)) {
         fixRecorder.WriteInputConform(rxargs.MsgStr_);
         FixBuilder fixb;
         RevPrint(fixb.GetBuffer(),
                  f9fix_kFLD_SessionRejectReason_ValueIsIncorrect
                  f9fix_SPLTAGEQ(Text) "NewSeqNo(", newSeqNo, ") < Expection(", msgSeqNum, ')');
         SendSessionReject(*rxargs.FixSender_, msgSeqNum, f9fix_kTAG_NewSeqNo, f9fix_kMSGTYPE_SequenceReset, std::move(fixb));
         return;
      }
      fixRecorder.WriteInputSeqReset(rxargs.MsgStr_, newSeqNo, isGapFill);
      if (rxargs.FixReceiver_->ResendRequestEndSeqNo_ < newSeqNo) {
         // 上次的 ResendRequest 補齊了! 檢查是否有保留訊息, 檢查是否已全部補齊.
         rxargs.FixReceiver_->ResendRequestEndSeqNo_ = 0;
         rxargs.FixReceiver_->CheckMsgKeeper(rxargs, newSeqNo);
      }
   }
   else { // SequenceReset: Reset mode.
      rxargs.FixReceiver_->ClearRecvKeeper();
      fixRecorder.WriteInputSeqReset(rxargs.MsgStr_, newSeqNo, isGapFill);
      rxargs.FixReceiver_->OnRecoverDone(rxargs);
   }
}
void FixReceiver::CheckMsgKeeper(const FixRecvEvArgs& rxargs, FixSeqNum newSeqNo) {
   // 若能從 this->MsgKeeper_ 取出保留的訊息, 則不用回補.
   // 若 this->MsgKeeper_ 裡面有 gap, 則針對 gap 要求回補.
   while (!this->MsgKeeper_.empty()) {
      const Msg& msg = this->MsgKeeper_.front();
      if (newSeqNo < msg.SeqNum_) {
         this->SendResendRequest(*rxargs.FixSender_, newSeqNo, msg.SeqNum_ - 1);
         return;
      }
      if (newSeqNo == msg.SeqNum_) {
         FixRecvEvArgs&  args = const_cast<FixRecvEvArgs&>(rxargs);
         StrView         fixmsg{&msg.MsgStr_};
         args.MsgStr_ = fixmsg;
         args.Msg_.Clear();
         args.Msg_.ParseFields(fixmsg, FixParser::Until::FullMessage);
         this->DispatchFixMessage(args);
         ++newSeqNo;
      }
      this->MsgKeeper_.pop_front();
   }
   if (newSeqNo < this->LastGapSeqNo_) {
      this->SendResendRequest(*rxargs.FixSender_, newSeqNo, this->LastGapSeqNo_);
      return;
   }
   // Ya! 補齊了!
   this->OnRecoverDone(rxargs);
}
void FixReceiver::OnRecoverDone(const FixRecvEvArgs& rxargs) {
   this->ResendRequestEndSeqNo_ = this->LastGapSeqNo_ = 0;
   FixRecorder& fixRecorder = rxargs.FixSender_->GetFixRecorder();
   fixRecorder.Write(f9fix_kCSTR_HdrInfo,
                     "OnRecoverDone:|NextRecvSeq=",
                     fixRecorder.GetNextRecvSeq());
}
void FixReceiver::SendResendRequest(FixSender& fixSender, FixSeqNum beginSeqNo, FixSeqNum endSeqNo) {
   this->ResendRequestEndSeqNo_ = endSeqNo;
   FixBuilder fixb;
   RevPrint(fixb.GetBuffer(),
            f9fix_SPLTAGEQ(BeginSeqNo), beginSeqNo,
            f9fix_SPLTAGEQ(EndSeqNo), endSeqNo);
   fixSender.Send(f9fix_SPLFLDMSGTYPE(ResendRequest), std::move(fixb));
}
//--------------------------------------------------------------------------//
void FixReceiver::OnFixMsgHandlerNotFound(const FixRecvEvArgs& rxargs, const StrView& msgType) {
   SendInvalidMsgType(*rxargs.FixSender_, rxargs.Msg_.GetMsgSeqNum(), msgType);
}
void FixReceiver::DispatchFixMessage(FixRecvEvArgs& rxargs) {
   assert(rxargs.FixReceiver_ == this);
   FixRecorder&               fixRecorder = rxargs.FixSender_->GetFixRecorder();
   const FixParser::FixField* fldMsgType = rxargs.Msg_.GetField(f9fix_kTAG_MsgType);
   const FixSeqNum            msgSeqNum = rxargs.Msg_.GetMsgSeqNum();
   if (fon9_UNLIKELY(!fldMsgType)) {
      fixRecorder.Write(f9fix_kCSTR_HdrError, rxargs.MsgStr_);
      SendRequiredTagMissing(*rxargs.FixSender_, msgSeqNum, f9fix_kTAG_MsgType, nullptr);
      return;
   }
   rxargs.ResetSeqSt();

   const StrView  msgType = fldMsgType->Value_;
   FixTag         errTag = this->CheckCompID(rxargs, msgType);
   if (fon9_UNLIKELY(errTag != 0))
      return;

   const FixMsgTypeConfig* cfg = rxargs.FixConfig_->Get(msgType);
   if (fon9_LIKELY(rxargs.SeqSt_ == FixSeqSt::Conform)) {
      if (fon9_LIKELY(cfg && cfg->FixMsgHandler_)) {
         if (fon9_LIKELY(!IsEnumContains(cfg->FixSeqAllow_, FixSeqSt::NoPreRecord)))
            fixRecorder.WriteInputConform(rxargs.MsgStr_);
         // 一般而言, 正常的連續訊息會來到這兒, 丟給 FixMsgHandler 處理!
         cfg->FixMsgHandler_(rxargs);
      }
      else {
         fixRecorder.WriteInputConform(rxargs.MsgStr_);
         this->OnFixMsgHandlerNotFound(rxargs, msgType);
      }
      this->OnMessageProcessed(rxargs, msgSeqNum);
      return;
   }
   if (cfg && cfg->FixMsgHandler_) {
      if (IsEnumContains(cfg->FixSeqAllow_, rxargs.SeqSt_)) {
         cfg->FixMsgHandler_(rxargs);
         return;
      }
   }
   bool isKeepRequired = cfg && cfg->IsInfiniteTTL(); // 永久有效的訊息(例如:ExecutionReport)才需要保留.
   this->OnMsgSeqNumNotExpected(rxargs, isKeepRequired ? GapKeepRequired : GapDontKeep);
}
void FixReceiver::OnMsgSeqNumNotExpected(const FixRecvEvArgs& rxargs, GapFlags flags) {
   if (rxargs.SeqSt_ == FixSeqSt::Conform)
      return;
   if (fon9_UNLIKELY(rxargs.SeqSt_ == FixSeqSt::TooLow)) {
      this->OnMsgSeqNumTooLow(rxargs);
      return;
   }
   FixRecorder&   fixRecorder = rxargs.FixSender_->GetFixRecorder();
   FixSeqNum      msgSeqNum = rxargs.Msg_.GetMsgSeqNum();
   // gap => ResendRequest.
   if (msgSeqNum <= this->LastGapSeqNo_) {
      // [回補範圍內] 的 [不連續訊息] => 不理會, 等候回補
      if (!(flags & GapSkipRecord))
         fixRecorder.Write(f9fix_kCSTR_HdrIgnoreRecv, rxargs.MsgStr_);
      return;
   }
   this->LastGapSeqNo_ = msgSeqNum;
   if (!(flags & GapKeepRequired)) {
      if (!(flags & GapSkipRecord))
         fixRecorder.Write(f9fix_kCSTR_HdrIgnoreRecv, rxargs.MsgStr_);
   }
   else {
      if (!(flags & GapSkipRecord))
         fixRecorder.Write(f9fix_kCSTR_HdrGapRecv, rxargs.MsgStr_);
      this->MsgKeeper_.emplace_back(rxargs.MsgStr_.ToString(), msgSeqNum);
      --msgSeqNum;
   }
   if (this->ResendRequestEndSeqNo_ > 0) // 回補中,等補到指定的序號時,再提出後續的回補要求.
      return;
   const FixSeqNum expectSeqNum = fixRecorder.GetNextRecvSeq();
   this->SendResendRequest(*rxargs.FixSender_, expectSeqNum, msgSeqNum);
}
void FixReceiver::OnMsgSeqNumTooLow(const FixRecvEvArgs& rxargs) {
   FixRecorder&   fixRecorder = rxargs.FixSender_->GetFixRecorder();
   if (const FixParser::FixField* fldPossDup = rxargs.Msg_.GetField(f9fix_kTAG_PossDupFlag))
      if (fldPossDup->Value_.Get1st() == 'Y') {
         fixRecorder.Write(f9fix_kCSTR_HdrIgnoreRecv, rxargs.MsgStr_);
         this->OnRecvPossDup(rxargs);
         return;
      }
   fixRecorder.Write(f9fix_kCSTR_HdrError, rxargs.MsgStr_);
   FixBuilder  fixb;
   RevPrint(fixb.GetBuffer(),
            f9fix_SPLTAGEQ(Text) // "|58="
            "MsgSeqNum too low, expecting ", fixRecorder.GetNextRecvSeq(),
            " but received ", rxargs.Msg_.GetMsgSeqNum());
   this->OnLogoutRequired(rxargs, std::move(fixb));
}

} } // namespaces
