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
void FixReceiver::OnRecvPossDup(const FixRecvEvArgs& args) {
   (void)args;
}
void FixReceiver::OnLogoutRequired(const FixRecvEvArgs& args, FixBuilder&& fixb) {
   args.FixSender_->Send(f9fix_SPLFLDMSGTYPE(Logout), std::move(fixb));
}
//--------------------------------------------------------------------------//
FixTag FixReceiver::CheckCompID(const FixRecvEvArgs& args, const StrView& msgType) {
   FixTag errTag = args.FixSender_->CompIDs_.Check(args.Msg_);
   if (errTag)
      this->OnBadCompID(args, errTag, msgType);
   return errTag;
}
void FixReceiver::OnBadCompID(const FixRecvEvArgs& args, FixTag errTag, const StrView& msgType) {
   FixRecorder&   fixRecorder = args.FixSender_->GetFixRecorder();
   // 為了避免對方系統出錯, 如果「只寫 err 序號不變」,
   // 則 ResendRequest 可能再次補到 CompID 有問題的訊息,
   // 造成訊息無法繼續下去.
   // 所以如果此筆的序號正確, 則使用 WriteInputConform(), 讓下一筆正確序號的訊息能進入系統.
   if (args.SeqSt_ == FixSeqSt::Conform)
      fixRecorder.WriteInputConform(args.MsgStr_);
   else
      fixRecorder.Write(f9fix_kCSTR_HdrError, args.MsgStr_);
   FixBuilder  fixb;
   RevPrint(fixb.GetBuffer(), f9fix_kFLD_SessionRejectReason_CompIDProblem f9fix_SPLTAGEQ(Text) "CompID problem");
   SendSessionReject(*args.FixSender_, args.Msg_.GetMsgSeqNum(), errTag, msgType, std::move(fixb));
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
void FixReceiver::OnRecvResendRequest(const FixRecvEvArgs& args) {
   if (args.SeqSt_ != FixSeqSt::Conform)
      args.FixSender_->GetFixRecorder().Write(f9fix_kCSTR_HdrReplay, args.MsgStr_);
   FixTag errTag;
   if (const FixParser::FixField* fldBeginSeqNo = args.Msg_.GetField(errTag = f9fix_kTAG_BeginSeqNo)) {
      if (const FixParser::FixField* fldEndSeqNo = args.Msg_.GetField(errTag = f9fix_kTAG_EndSeqNo)) {
         FixSeqNum beginSeqNo = StrTo(fldBeginSeqNo->Value_, FixSeqNum{});
         FixSeqNum endSeqNo = StrTo(fldEndSeqNo->Value_, FixSeqNum{});
         args.FixSender_->Replay(*args.FixConfig_, beginSeqNo, endSeqNo);
         if (args.SeqSt_ != FixSeqSt::Conform)
            args.FixReceiver_->OnMsgSeqNumNotExpected(args, FixReceiver::GapSkipRecord);
         return;
      }
   }
   SendRequiredTagMissing(*args.FixSender_, args.Msg_.GetMsgSeqNum(), errTag, f9fix_kMSGTYPE_ResendRequest);
   if (args.SeqSt_ != FixSeqSt::Conform)
      args.FixReceiver_->OnMsgSeqNumNotExpected(args, FixReceiver::GapSkipRecord);
}
void FixReceiver::OnRecvSequenceReset(const FixRecvEvArgs& args) {
   bool isGapFill = false;
   if (const FixParser::FixField* fldGapFill = args.Msg_.GetField(f9fix_kTAG_GapFillFlag))
      isGapFill = (fldGapFill->Value_.Get1st() == 'Y');

   if (isGapFill && args.SeqSt_ != FixSeqSt::Conform) {
      // GapFill 必須是正確序號(msgSeqNum).
      args.FixReceiver_->OnMsgSeqNumNotExpected(args, GapDontKeep);
      return;
   }

   FixRecorder&     fixRecorder = args.FixSender_->GetFixRecorder();
   const FixSeqNum  msgSeqNum = args.Msg_.GetMsgSeqNum();
   const FixParser::FixField* fldNewSeqNo = args.Msg_.GetField(f9fix_kTAG_NewSeqNo);
   if (fldNewSeqNo == nullptr) {
      fixRecorder.Write(f9fix_kCSTR_HdrError, args.MsgStr_);
      SendRequiredTagMissing(*args.FixSender_, msgSeqNum, f9fix_kTAG_NewSeqNo, f9fix_kMSGTYPE_SequenceReset);
      return;
   }
   FixSeqNum newSeqNo = StrTo(fldNewSeqNo->Value_, FixSeqNum{0});

   if (isGapFill) {
      if (fon9_UNLIKELY(newSeqNo < msgSeqNum)) {
         fixRecorder.WriteInputConform(args.MsgStr_);
         FixBuilder fixb;
         RevPrint(fixb.GetBuffer(),
                  f9fix_kFLD_SessionRejectReason_ValueIsIncorrect
                  f9fix_SPLTAGEQ(Text) "NewSeqNo(", newSeqNo, ") < Expection(", msgSeqNum, ')');
         SendSessionReject(*args.FixSender_, msgSeqNum, f9fix_kTAG_NewSeqNo, f9fix_kMSGTYPE_SequenceReset, std::move(fixb));
         return;
      }
      fixRecorder.WriteInputSeqReset(args.MsgStr_, newSeqNo, isGapFill);
      if (args.FixReceiver_->ResendRequestEndSeqNo_ < newSeqNo) {
         // 上次的 ResendRequest 補齊了! 檢查是否有保留訊息, 檢查是否已全部補齊.
         args.FixReceiver_->ResendRequestEndSeqNo_ = 0;
         args.FixReceiver_->CheckMsgKeeper(args, newSeqNo);
      }
   }
   else { // SequenceReset: Reset mode.
      args.FixReceiver_->ClearRecvKeeper();
      fixRecorder.WriteInputSeqReset(args.MsgStr_, newSeqNo, isGapFill);
      args.FixReceiver_->OnRecoverDone(args);
   }
}
void FixReceiver::CheckMsgKeeper(const FixRecvEvArgs& args, FixSeqNum newSeqNo) {
   // 若能從 this->MsgKeeper_ 取出保留的訊息, 則不用回補.
   // 若 this->MsgKeeper_ 裡面有 gap, 則針對 gap 要求回補.
   while (!this->MsgKeeper_.empty()) {
      const Msg& msg = this->MsgKeeper_.front();
      if (newSeqNo < msg.SeqNum_) {
         this->SendResendRequest(*args.FixSender_, newSeqNo, msg.SeqNum_ - 1);
         return;
      }
      if (newSeqNo == msg.SeqNum_) {
         FixRecvEvArgs&  rxarg = const_cast<FixRecvEvArgs&>(args);
         StrView         fixmsg{&msg.MsgStr_};
         rxarg.MsgStr_ = fixmsg;
         rxarg.Msg_.Clear();
         rxarg.Msg_.ParseFields(fixmsg, FixParser::Until::FullMessage);
         this->Receive(rxarg);
         ++newSeqNo;
      }
      this->MsgKeeper_.pop_front();
   }
   if (newSeqNo < this->LastGapSeqNo_) {
      this->SendResendRequest(*args.FixSender_, newSeqNo, this->LastGapSeqNo_);
      return;
   }
   // Ya! 補齊了!
   this->OnRecoverDone(args);
}
void FixReceiver::OnRecoverDone(const FixRecvEvArgs& args) {
   this->ResendRequestEndSeqNo_ = this->LastGapSeqNo_ = 0;
   FixRecorder& fixRecorder = args.FixSender_->GetFixRecorder();
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
void FixReceiver::OnFixMsgHandlerNotFound(const FixRecvEvArgs& args, const StrView& msgType) {
   SendInvalidMsgType(*args.FixSender_, args.Msg_.GetMsgSeqNum(), msgType);
}
void FixReceiver::Receive(FixRecvEvArgs& args) {
   assert(args.FixReceiver_ == this);
   FixRecorder&               fixRecorder = args.FixSender_->GetFixRecorder();
   const FixParser::FixField* fldMsgType = args.Msg_.GetField(f9fix_kTAG_MsgType);
   const FixSeqNum            msgSeqNum = args.Msg_.GetMsgSeqNum();
   if (fon9_UNLIKELY(!fldMsgType)) {
      fixRecorder.Write(f9fix_kCSTR_HdrError, args.MsgStr_);
      SendRequiredTagMissing(*args.FixSender_, msgSeqNum, f9fix_kTAG_MsgType, nullptr);
      return;
   }
   args.SeqSt_ = CompareFixSeqNum(msgSeqNum , fixRecorder.GetNextRecvSeq());

   const StrView  msgType = fldMsgType->Value_;
   FixTag         errTag = this->CheckCompID(args, msgType);
   if (fon9_UNLIKELY(errTag != 0))
      return;

   const FixMsgTypeConfig* cfg = args.FixConfig_->Get(msgType);
   if (fon9_LIKELY(args.SeqSt_ == FixSeqSt::Conform)) {
      if (fon9_LIKELY(cfg && cfg->FixMsgHandler_)) {
         if (fon9_LIKELY(!IsEnumContains(cfg->FixSeqAllow_, FixSeqSt::NoPreRecord)))
            fixRecorder.WriteInputConform(args.MsgStr_);
         // 一般而言, 正常的連續訊息會來到這兒, 丟給 FixMsgHandler 處理!
         cfg->FixMsgHandler_(args);
      }
      else {
         fixRecorder.WriteInputConform(args.MsgStr_);
         this->OnFixMsgHandlerNotFound(args, msgType);
      }
      this->OnMessageProcessed(args, msgSeqNum);
      return;
   }
   if (cfg && cfg->FixMsgHandler_) {
      if (IsEnumContains(cfg->FixSeqAllow_, args.SeqSt_)) {
         cfg->FixMsgHandler_(args);
         return;
      }
   }
   bool isKeepRequired = cfg && cfg->IsInfiniteTTL(); // 永久有效的訊息(例如:ExecutionReport)才需要保留.
   this->OnMsgSeqNumNotExpected(args, isKeepRequired ? GapKeepRequired : GapDontKeep);
}
void FixReceiver::OnMsgSeqNumNotExpected(const FixRecvEvArgs& args, GapFlags flags) {
   if (args.SeqSt_ == FixSeqSt::Conform)
      return;
   if (fon9_UNLIKELY(args.SeqSt_ == FixSeqSt::TooLow)) {
      this->OnMsgSeqNumTooLow(args);
      return;
   }
   FixRecorder&   fixRecorder = args.FixSender_->GetFixRecorder();
   FixSeqNum      msgSeqNum = args.Msg_.GetMsgSeqNum();
   // gap => ResendRequest.
   if (msgSeqNum <= this->LastGapSeqNo_) {
      // [回補範圍內] 的 [不連續訊息] => 不理會, 等候回補
      if (!(flags & GapSkipRecord))
         fixRecorder.Write(f9fix_kCSTR_HdrIgnoreRecv, args.MsgStr_);
      return;
   }
   this->LastGapSeqNo_ = msgSeqNum;
   if (!(flags & GapKeepRequired)) {
      if (!(flags & GapSkipRecord))
         fixRecorder.Write(f9fix_kCSTR_HdrIgnoreRecv, args.MsgStr_);
   }
   else {
      if (!(flags & GapSkipRecord))
         fixRecorder.Write(f9fix_kCSTR_HdrGapRecv, args.MsgStr_);
      this->MsgKeeper_.emplace_back(args.MsgStr_.ToString(), msgSeqNum);
      --msgSeqNum;
   }
   if (this->ResendRequestEndSeqNo_ > 0) // 回補中,等補到指定的序號時,再提出後續的回補要求.
      return;
   const FixSeqNum expectSeqNum = fixRecorder.GetNextRecvSeq();
   this->SendResendRequest(*args.FixSender_, expectSeqNum, msgSeqNum);
}
void FixReceiver::OnMsgSeqNumTooLow(const FixRecvEvArgs& args) {
   FixRecorder&   fixRecorder = args.FixSender_->GetFixRecorder();
   if (const FixParser::FixField* fldPossDup = args.Msg_.GetField(f9fix_kTAG_PossDupFlag))
      if (fldPossDup->Value_.Get1st() == 'Y') {
         fixRecorder.Write(f9fix_kCSTR_HdrIgnoreRecv, args.MsgStr_);
         this->OnRecvPossDup(args);
         return;
      }
   fixRecorder.Write(f9fix_kCSTR_HdrError, args.MsgStr_);
   FixBuilder  fixb;
   RevPrint(fixb.GetBuffer(),
            f9fix_SPLTAGEQ(Text) // "|58="
            "MsgSeqNum too low, expecting ", fixRecorder.GetNextRecvSeq(),
            " but received ", args.Msg_.GetMsgSeqNum());
   this->OnLogoutRequired(args, std::move(fixb));
}

} } // namespaces
