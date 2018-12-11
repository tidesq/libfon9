/// \file fon9/fix/FixAdminDef.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_fix_FixAdminDef_hpp__
#define __fon9_fix_FixAdminDef_hpp__
#include "fon9/fix/FixBase.hpp"

namespace fon9 { namespace fix {

#define f9fix_kMSGTYPE_Heartbeat       "0"

#define f9fix_kMSGTYPE_TestRequest     "1"
#define f9fix_kTAG_TestReqID           112

#define f9fix_kMSGTYPE_ResendRequest   "2"
#define f9fix_kTAG_BeginSeqNo            7
#define f9fix_kTAG_EndSeqNo             16

#define f9fix_kMSGTYPE_SessionReject   "3"
#define f9fix_kTAG_RefTagID            371
#define f9fix_kTAG_SessionRejectReason 373
#define f9fix_kFLD_SessionRejectReason_InvalidTag                    f9fix_SPLTAGEQ(SessionRejectReason) "0"
#define f9fix_kFLD_SessionRejectReason_RequiredTagMissing            f9fix_SPLTAGEQ(SessionRejectReason) "1"
#define f9fix_kFLD_SessionRejectReason_TagNotDefinedForThisMsgType   f9fix_SPLTAGEQ(SessionRejectReason) "2"
#define f9fix_kFLD_SessionRejectReason_UndefinedTag                  f9fix_SPLTAGEQ(SessionRejectReason) "3"
#define f9fix_kFLD_SessionRejectReason_TagSpecifiedWithoutValue      f9fix_SPLTAGEQ(SessionRejectReason) "4"
#define f9fix_kFLD_SessionRejectReason_ValueIsIncorrect              f9fix_SPLTAGEQ(SessionRejectReason) "5" // (out of range) for this tag
#define f9fix_kFLD_SessionRejectReason_IncorrectDataFormat           f9fix_SPLTAGEQ(SessionRejectReason) "6" // for value
#define f9fix_kFLD_SessionRejectReason_DecryptionProblem             f9fix_SPLTAGEQ(SessionRejectReason) "7"
#define f9fix_kFLD_SessionRejectReason_SignatureProblem              f9fix_SPLTAGEQ(SessionRejectReason) "8" // Tag#89
#define f9fix_kFLD_SessionRejectReason_CompIDProblem                 f9fix_SPLTAGEQ(SessionRejectReason) "9"
#define f9fix_kFLD_SessionRejectReason_SendingTimeAccuracyProblem    f9fix_SPLTAGEQ(SessionRejectReason) "10"// Tag#52
#define f9fix_kFLD_SessionRejectReason_InvalidMsgType                f9fix_SPLTAGEQ(SessionRejectReason) "11"// Tag#35

#define f9fix_kMSGTYPE_SequenceReset   "4"
#define f9fix_kTAG_GapFillFlag         123
#define f9fix_kTAG_NewSeqNo             36

#define f9fix_kMSGTYPE_Logout          "5"

#define f9fix_kMSGTYPE_Logon           "A"
#define f9fix_kTAG_HeartBtInt          108
#define f9fix_kTAG_EncryptMethod        98
#define f9fix_kCSTR_EncryptMethod_None "0"
#define f9fix_kFLD_EncryptMethod_None  f9fix_SPLTAGEQ(EncryptMethod) f9fix_kCSTR_EncryptMethod_None

} } // namespaces
#endif//__fon9_fix_FixAdminDef_hpp__
