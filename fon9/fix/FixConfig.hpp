/// \file fon9/fix/FixConfig.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_fix_FixConfig_hpp__
#define __fon9_fix_FixConfig_hpp__
#include "fon9/fix/FixBase.hpp"
#include "fon9/TimeStamp.hpp"
#include "fon9/StrTools.hpp"
#include "fon9/SortedVector.hpp"

namespace fon9 { namespace fix {

/// \ingroup fix
/// FIX 序號的狀態, 用來協助「判斷、控制」序號連續性的處理行為.
enum class FixSeqSt {
   /// 序號必須符合規範才需要呼叫 FixMsgHandler.
   Conform = 0x00,
   TooHigh = 0x01,
   TooLow = 0x02,

   /// 不論序號是否符合規範, 訊息直接由 FixMsgHandler 處理.
   AllowAnySeq = TooHigh | TooLow,

   /// 呼叫 FixMsgHandler 之前, 完全不寫任何記錄(即使序號符合規範).
   /// 如果 FixMsgTypeConfig::FixSeqAllow_ 設定了此旗標,
   /// 則在 FixMsgHandler 處理 FixSeqSt::Conform 通知時,
   /// 應呼叫 `fixRecorder.WriteInputConform(args.MsgStr_);` 才能正確處理接收序號.
   /// 一般而言只有 SequenceReset 才會用到.
   NoPreRecord = 0x10,
};
fon9_ENABLE_ENUM_BITWISE_OP(FixSeqSt);
inline FixSeqSt CompareFixSeqNum(FixSeqNum rxMsgSeqNum, FixSeqNum expectSeqNum) {
   return (rxMsgSeqNum == expectSeqNum) ? FixSeqSt::Conform
        : (rxMsgSeqNum <  expectSeqNum) ? FixSeqSt::TooLow
        :                                 FixSeqSt::TooHigh;
}

fon9_WARN_DISABLE_PADDING;
struct FixRecvEvArgs {
   fon9_NON_COPY_NON_MOVE(FixRecvEvArgs);
   FixRecvEvArgs(FixParser& fixParser)
      : Msg_(fixParser) {
   }
   /// 已解析完畢的 FIX Message.
   FixParser&  Msg_;
   /// FIX Message.
   StrView     MsgStr_;
   /// 在呼叫 FixMsgHandler 時, 這裡會提供:
   /// - FixSeqSt::Conform 序號符合規範
   /// - FixSeqSt::TooHigh 如果序號高於預期, 且 FixMsgTypeConfig::FixSeqAllow_ 有設定 FixSeqSt::TooHigh
   /// - FixSeqSt::TooLow  如果序號高於預期, 且 FixMsgTypeConfig::FixSeqAllow_ 有設定 FixSeqSt::TooLow
   FixSeqSt    SeqSt_;

   /// 在事件通知期間, Session_ 必定有效.
   FixSession*    FixSession_;
   /// 在事件通知期間, Sender_ 必定有效.
   FixSender*     FixSender_;
   /// 在事件通知期間, Receiver_ 必定有效.
   FixReceiver*   FixReceiver_;
   /// 在事件通知期間, Config_ 必定有效.
   FixConfig*     FixConfig_;
};
using FixMsgHandler = std::function<void(const FixRecvEvArgs& rxarg)>;

struct FixOrigArgs {
   fon9_NON_COPY_NON_MOVE(FixOrigArgs);
   FixParser&  Msg_;
   StrView     MsgStr_;
   FixOrigArgs(FixParser& msgpr, const StrView& msgStr) : Msg_(msgpr), MsgStr_{msgStr} {
   }
};
using FixRejectHandler = std::function<void(const FixRecvEvArgs& rxarg, const FixOrigArgs& orig)>;

/// \ingroup fix
struct FixMsgTypeConfig {
   /// 訊息有效時間.
   /// - <=0 送出後立即失效(例如: Admin message).
   /// - IsNull() 永久有效(例如: ExecutionReport)
   /// - FixSender: 決定是否需要 Replay.
   /// - FixReceiver: 若有 gap 則依此判斷是否需要保留(永久有效的才需要保留).
   TimeInterval   TTL_;
   bool IsInfiniteTTL() const {
      return this->TTL_.IsNull();
   }
   void SetInfiniteTTL() {
      this->TTL_ = TimeInterval::Null();
   }

   /// 當訊息不連續時, 是否可以呼叫 FixMsgHandler_.
   /// - 當序號連續時, 呼叫前必定會先寫 fixRecorder.WriteInputConform(args.MsgStr_);
   /// - 不連續時, 則必須由 FixMsgHandler_ 自行記錄必要訊息.
   FixSeqSt          FixSeqAllow_{FixSeqSt::Conform};
   /// 當 FixReceiver 收到一筆可用訊息時, 透過這裡通知.
   FixMsgHandler     FixMsgHandler_;
   /// 當 FixReceiver 收到 SessionReject or BusinessReject 時.
   /// 根據原訊息的 MsgType 決定後續處理.
   FixRejectHandler  FixRejectHandler_;
};

/// \ingroup fix
/// FIX 執行環境設定.
/// - 根據 MsgType 設定處理程序.
/// - Session 相關參數:
///   - Logon 參數: 連線成功後多久內要登入成功?
///   - Hb 相關參數.
/// - 沒有任何 Lock:
///   - 通常在初始化階段進行設定.
///   - 初始化完成後, 就不會再調整設定.
class fon9_API FixConfig {
   using ConfigMap = SortedVector<CharVector, FixMsgTypeConfig>;
   ConfigMap   ConfigMap_;
   // 單一字元的 MsgType(A..Z,a..z,0..9) 使用此對照表.
   FixMsgTypeConfig  Configs_[kSeq2AlphaSize];

public:
   /// 因為不提供任何 lock 保護,
   /// 所以僅允許在系統初始階段在單一 thread 裡面進行設定.
   FixMsgTypeConfig& Fetch(StrView msgType);

   /// 根據 msgType 取得相關設定.
   const FixMsgTypeConfig* Get(StrView msgType) const;

   /// 從 LinkReady 到收到登入訊息, 如果超過此時間, 則斷線.
   TimeInterval   TiWaitForLogon_{TimeInterval_Second(3)};
   /// 收到的 Logon 訊息有 gap, 要求對方重送, 超過此時間, 則斷線.
   /// - 如果在斷線前, 有收到任何有效訊息, 則時間重算.
   TimeInterval   TiLogonRecover_{TimeInterval_Second(10)};
   /// 當 Logon 的 gap 補齊了, 會送一個 TestRequest 給對方:
   /// - 如果對方有回應 Hb, 則表示可進入 ApReady 狀態.
   /// - 如果對方沒回應 Hb, 則可能對方還在 ResendRequest.
   ///   - 如果超過此時間沒回應, 則會再送一次 TestRequest.
   ///   - 如果送出的 TestRequest 次數 >= MaxLogonTestCount_ 則斷線.
   ///     - 在超過次數之前, 有收到任何有效訊息, 則次數歸零重算.
   TimeInterval   TiLogonTest_{TimeInterval_Second(1)};
   uint32_t       MaxLogonTestCount_{10};

   /// 在 ApReady 狀態, 每 heartBtInt秒 之後送一個 Hb,
   /// 在送出 HbTestRequestCount_ 次 Hb 之後:
   /// - 如果對方沒有送來任何訊息, 則送出一個 TestRequest.
   /// - 如果在下次 Hb 之前, 仍沒收到任何訊息, 則斷線.
   uint32_t       HbTestRequestCount_{2};

   /// 送出 Logout 之後, 多久要斷線.
   TimeInterval   TiLogoutPending_{TimeInterval_Second(10)};

   /// 不論各 MsgType 設定的 TTL_, 使用此 FixConfig 的 Session, 一律不考慮 Replay.
   /// 當有 Replay 的需求時(FixSender::Replay), 一律使用 FixSender::GapFill.
   /// 例如: 券商與交易所之間的連線, 券商端斷線後重連, 可能全都不重送.
   bool  IsNoReplay_{false};
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_fix_FixConfig_hpp__
