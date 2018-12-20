/// \file fon9/fix/FixBusinessReject.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_fix_FixBusinessReject_hpp__
#define __fon9_fix_FixBusinessReject_hpp__
#include "fon9/fix/FixConfig.hpp"

namespace fon9 { namespace fix {

#define f9fix_kMSGTYPE_BusinessReject     "j"
#define f9fix_kTAG_BusinessRejectRefID    379
#define f9fix_kTAG_BusinessRejectReason   380
#define f9fix_kFLD_BusinessRejectReason_Other                     f9fix_SPLTAGEQ(BusinessRejectReason) "0"
#define f9fix_kFLD_BusinessRejectReason_UnkownID                  f9fix_SPLTAGEQ(BusinessRejectReason) "1"
#define f9fix_kFLD_BusinessRejectReason_UnknownSecurity           f9fix_SPLTAGEQ(BusinessRejectReason) "2"
#define f9fix_kFLD_BusinessRejectReason_UnsupportedMsgType        f9fix_SPLTAGEQ(BusinessRejectReason) "3"
#define f9fix_kFLD_BusinessRejectReason_AppNotAvailable           f9fix_SPLTAGEQ(BusinessRejectReason) "4"
#define f9fix_kFLD_BusinessRejectReason_CondRequiredFieldMissing  f9fix_SPLTAGEQ(BusinessRejectReason) "5"

/// \ingroup fix
/// 送出 BusinessReject: Unsupported MsgType
fon9_API void SendUnsupportedMsgType(FixSender& fixout, FixSeqNum refSeqNum, StrView refMsgType);

/// \ingroup fix
/// 處理: SessionReject, BusinessReject.
/// - 透過 rxargs.Msg_.GetField(f9fix_kTAG_RefSeqNum); 取回原本送出的 orig 訊息.
/// - 然後透過 cfg->FixRejectHandler_(rxargs, orig) 處理.
fon9_API void OnRecvRejectMessage(const FixRecvEvArgs& rxargs);

/// \ingroup fix
/// 設定 MsgType:
/// - SessionReject: 允許 FixSeqSt::TooHigh
/// - BusinessReject: 不改變 FixSeqSt, 預設為 FixSeqSt::Conform
fon9_API void InitRecvRejectMessage(FixConfig& cfg);

} } // namespaces
#endif//__fon9_fix_FixBusinessReject_hpp__
