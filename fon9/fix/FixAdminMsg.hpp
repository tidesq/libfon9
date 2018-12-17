/// \file fon9/fix/FixAdminMsg.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_fix_FixAdmin_hpp__
#define __fon9_fix_FixAdmin_hpp__
#include "fon9/fix/FixAdminDef.hpp"
#include "fon9/fix/FixConfig.hpp"
#include "fon9/fix/FixParser.hpp"

namespace fon9 { namespace fix {

/// \ingroup fix
/// 送出 SessionReject: RequiredTagMissing
fon9_API void SendRequiredTagMissing(FixSender& fixSender, FixSeqNum refSeqNum, FixTag refTagID, StrView refMsgType);

/// \ingroup fix
/// 送出 SessionReject: fixb = 必須已填妥 RejectReason, 例如:
/// \code
///   FixBuilder fixb;
///   RevPutStr(fixb.GetBuffer(),
///         f9fix_kFLD_SessionRejectReason_ValueIsIncorrect
///         f9fix_SPLTAGEQ(Text) "Reject reason text");
///   SendSessionReject(fixSender, refSeqNum, refTagID, refMsgType, std::move(fixb));
/// \endcode
fon9_API void SendSessionReject(FixSender& fixSender, FixSeqNum refSeqNum, FixTag refTagID, StrView refMsgType, FixBuilder&& fixb);

/// \ingroup fix
/// 送出 SessionReject: InvalidMsgType
fon9_API void SendInvalidMsgType(FixSender& fixSender, FixSeqNum refSeqNum, StrView refMsgType);

/// \ingroup fix
/// fldTestReqID==nullptr 表示不用 TestReqID 欄位.
fon9_API void SendHeartbeat(FixSender& fixSender, const FixParser::FixField* fldTestReqID);

} } // namespaces
#endif//__fon9_fix_FixAdminMsg_hpp__
