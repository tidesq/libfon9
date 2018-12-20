// \file fon9/fix/FixBusinessReject.cpp
// \author fonwinz@gmail.com
#include "fon9/fix/FixBusinessReject.hpp"
#include "fon9/fix/FixSender.hpp"
#include "fon9/fix/FixAdminDef.hpp"

namespace fon9 { namespace fix {

fon9_API void SendUnsupportedMsgType(FixSender& fixout, FixSeqNum refSeqNum, StrView refMsgType) {
   FixBuilder fixb;
   RevPrint(fixb.GetBuffer(),
            f9fix_SPLTAGEQ(RefSeqNum), refSeqNum,
            f9fix_SPLTAGEQ(RefMsgType), refMsgType,
            f9fix_kFLD_BusinessRejectReason_UnsupportedMsgType
            f9fix_SPLTAGEQ(Text) "Unsupported MsgType:", refMsgType);
   fixout.Send(f9fix_SPLFLDMSGTYPE(BusinessReject), std::move(fixb));
}

fon9_API void OnRecvRejectMessage(const FixRecvEvArgs& rxargs) {
   const FixParser::FixField* fldRefSeqNum = rxargs.Msg_.GetField(f9fix_kTAG_RefSeqNum);
   if (fldRefSeqNum == nullptr) {
      // Send.SessionReject/BusinessReject: Required tag(RefSeqNum) missing?
      return;
   }
   // SessionReject/BusinessReject 不會經常發生(不用考慮效率), 所以直接取回原始資料.
   FixSeqNum               refSeqNum = StrTo(fldRefSeqNum->Value_, FixSeqNum{});
   FixRecorder&            fixr = rxargs.FixSender_->GetFixRecorder();
   FixRecorder::ReloadSent reloader;
   StrView                 fixmsg{reloader.Find(fixr, refSeqNum)};
   if (fon9_UNLIKELY(fixmsg.empty())) {
      fixr.Write(f9fix_kCSTR_HdrError, "RecvReject: Cannot find orig message.");
      return;
   }
   FixOrigArgs orig{reloader.FixParser_, fixmsg};
   orig.Msg_.Parse(fixmsg);
   const FixParser::FixField* fldMsgType = orig.Msg_.GetField(f9fix_kTAG_MsgType);
   if (fon9_UNLIKELY(fldMsgType == nullptr)) {
      fixr.Write(f9fix_kCSTR_HdrError, "RecvReject: Bad orig message: no MsgType.");
      return;
   }
   const FixMsgTypeConfig* cfg = rxargs.FixConfig_->Get(fldMsgType->Value_);
   if (fon9_UNLIKELY(cfg == nullptr)) {
      fixr.Write(f9fix_kCSTR_HdrError, "RecvReject: No orig MsgType config.");
      return;
   }
   if (cfg->FixRejectHandler_)
      cfg->FixRejectHandler_(rxargs, orig);
   else
      fixr.Write(f9fix_kCSTR_HdrError, "RecvReject: No orig MsgType handler.");
}
fon9_API void InitRecvRejectMessage(FixConfig& cfg) {
   FixMsgTypeConfig* mcfg;
   mcfg = &cfg.Fetch(f9fix_kMSGTYPE_SessionReject);
   mcfg->FixSeqAllow_ = FixSeqSt::TooHigh;
   mcfg->FixMsgHandler_ = &OnRecvRejectMessage;

   cfg.Fetch(f9fix_kMSGTYPE_BusinessReject).FixMsgHandler_ = &OnRecvRejectMessage;
}

} } // namespaces
