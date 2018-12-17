// \file fon9/fix/FixAdminMsg.cpp
// \author fonwinz@gmail.com
#include "fon9/fix/FixAdminMsg.hpp"
#include "fon9/fix/FixSender.hpp"

namespace fon9 { namespace fix {

fon9_API void SendRequiredTagMissing(FixSender& fixSender, FixSeqNum refSeqNum, FixTag refTagID, StrView refMsgType) {
   FixBuilder fixb;
   RevPrint(fixb.GetBuffer(),
            f9fix_SPLTAGEQ(RefSeqNum), refSeqNum,
            f9fix_SPLTAGEQ(RefTagID), refTagID,
            f9fix_kFLD_SessionRejectReason_RequiredTagMissing
            f9fix_SPLTAGEQ(Text) "Required tag missing:", refTagID);
   if (!refMsgType.empty())
      RevPrint(fixb.GetBuffer(), f9fix_SPLTAGEQ(RefMsgType), refMsgType);
   fixSender.Send(f9fix_SPLFLDMSGTYPE(SessionReject), std::move(fixb));
}
fon9_API void SendSessionReject(FixSender& fixSender, FixSeqNum refSeqNum, FixTag refTagID, StrView refMsgType, FixBuilder&& fixb) {
   if(refTagID)
      RevPrint(fixb.GetBuffer(), f9fix_SPLTAGEQ(RefTagID), refTagID);
   RevPrint(fixb.GetBuffer(),
            f9fix_SPLTAGEQ(RefMsgType), refMsgType,
            f9fix_SPLTAGEQ(RefSeqNum), refSeqNum);
   fixSender.Send(f9fix_SPLFLDMSGTYPE(SessionReject), std::move(fixb));
}
fon9_API void SendInvalidMsgType(FixSender& fixSender, FixSeqNum refSeqNum, StrView refMsgType) {
   FixBuilder fixb;
   RevPrint(fixb.GetBuffer(),
            f9fix_SPLTAGEQ(RefSeqNum), refSeqNum,
            f9fix_SPLTAGEQ(RefMsgType), refMsgType,
            f9fix_kFLD_SessionRejectReason_InvalidMsgType
            f9fix_SPLTAGEQ(Text) "Invalid MsgType:", refMsgType);
   fixSender.Send(f9fix_SPLFLDMSGTYPE(SessionReject), std::move(fixb));
}

fon9_API void SendHeartbeat(FixSender& fixSender, const FixParser::FixField* fldTestReqID) {
   FixBuilder fixb;
   if (fldTestReqID)
      RevPrint(fixb.GetBuffer(), f9fix_SPLTAGEQ(TestReqID), fldTestReqID->Value_);
   fixSender.Send(f9fix_SPLFLDMSGTYPE(Heartbeat), std::move(fixb));
}

} } // namespaces
