/// \file fon9/fix/FixBase.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_fix_FixBase_hpp__
#define __fon9_fix_FixBase_hpp__
#include "fon9/intrusive_ref_counter.hpp"

namespace fon9 { namespace fix {

#define f9fix_kCSTR_SPL                      "\x01"
#define f9fix_kCHAR_SPL                      '\x01'

#define f9fix_kTAG_BeginString                8
#define f9fix_kTAG_BodyLength                 9
#define f9fix_kTAG_CheckSum                  10
#define f9fix_kTAG_MsgSeqNum                 34
#define f9fix_kTAG_MsgType                   35
#define f9fix_kTAG_SendingTime               52

#define f9fix_kTAG_PossDupFlag               43
#define f9fix_kTAG_OrigSendingTime          122

#define f9fix_kTAG_SenderCompID              49
#define f9fix_kTAG_SenderSubID               50
#define f9fix_kTAG_TargetCompID              56
#define f9fix_kTAG_TargetSubID               57

#define f9fix_kTAG_RawData                   96
#define f9fix_kTAG_RawDataLength             95

#define f9fix_kTAG_LastMsgSeqNumProcessed   369
#define f9fix_kTAG_Text                      58
#define f9fix_kTAG_EncodedTextLen           354
#define f9fix_kTAG_EncodedText              355

#define f9fix_kTAG_RefSeqNum                 45
#define f9fix_kTAG_RefMsgType               372

/// \ingroup fix
/// 建立 FIX tag 的字串, 例如:
/// - 必須要先定義 \#`define f9fix_kTAG_BeginString  8`
/// - `f9fix_STRTAG(BeginString)` 會得到字串: "8"
#define f9fix_STRTAG(tagName)    fon9_CTXTOCSTR(f9fix_kTAG_##tagName)

/// \ingroup fix
/// 建立包含開始的「分隔字元」+「FIX tag」+「=」的字串, 例如:
/// - `f9fix_FTAG(SenderCompID)` 會得到字串: "\x01" "49" "="
#define f9fix_SPLTAGEQ(tagName)  \
         f9fix_kCSTR_SPL         \
         f9fix_STRTAG(tagName)   \
         "="

/// \ingroup fix
/// 建立 "8=beginString|9=" 的 FIX Message 開始字串.
/// 範例: `f9fix_BEGIN_HEADER("FIX.4.4")`
#define f9fix_BEGIN_HEADER(verValue)   \
         f9fix_STRTAG(BeginString)     \
         "="                           \
         verValue                      \
         f9fix_SPLTAGEQ(BodyLength)

#define f9fix_BEGIN_HEADER_V44   f9fix_BEGIN_HEADER("FIX.4.4")
#define f9fix_BEGIN_HEADER_V42   f9fix_BEGIN_HEADER("FIX.4.2")

/// \ingroup fix
/// 建立 "\x01" "35=" msgType 的 FIX 欄位字串.
/// - 範例: `f9fix_SPLFLDMSGTYPE(NewOrderSingle)` 結果為 "|35=D"
/// - 必須要先定義 \#`define f9fix_kMSGTYPE_NewOrderSingle "D"` 才會正確
#define f9fix_SPLFLDMSGTYPE(msgType)  \
         f9fix_SPLTAGEQ(MsgType)   \
         f9fix_kMSGTYPE_##msgType

class fon9_API FixParser;
class fon9_API FixBuilder;
class fon9_API FixConfig;
class fon9_API FixSender;
class fon9_API FixReceiver;

class fon9_API FixManager;
class fon9_API FixSession;
using FixSessionSP = intrusive_ptr<FixSession>;

using FixSeqNum = uint32_t;
using FixTag = uint32_t;

enum class FixVersion {
   V44 = 44,
   V42 = 42,
};

enum : uint32_t {
   kFixTailWidth = static_cast<uint32_t>(sizeof("|10=xxx|") - 1),
   kFixMinHeaderWidth = static_cast<uint32_t>(sizeof("8=FIX.4.4|9=xxx|") - 1),
   kFixMinMessageWidth = kFixMinHeaderWidth + kFixTailWidth,

   /// 為了安全, 設定一個可忍受的最大長度, 超過此長度則視為無效的 FIX 訊息.
   /// 在 FixParser::Verify() 處理 bodyLength > kFixMaxBodyLength 時,
   /// 返回 FixParser::EOverFixMaxBodyLength
   kFixMaxBodyLength = 1024 * 1024,
};

} } // namespaces
#endif//__fon9_fix_FixBase_hpp__
