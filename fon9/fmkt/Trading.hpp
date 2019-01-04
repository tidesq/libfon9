/// \file fon9/fmkt/Trading.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_Trading_hpp__
#define __fon9_fmkt_Trading_hpp__
#include "fon9/Timer.hpp"

namespace fon9 { namespace fmkt {

class fon9_API TradingLineManager;

/// \ingroup fmkt
/// 下單要求基底.
class fon9_API TradingRequest {
   fon9_NON_COPY_NON_MOVE(TradingRequest);
public:
   virtual ~TradingRequest();
};

/// \ingroup fmkt
/// 下單要求傳送的結果.
enum class SendRequestResult : TimeInterval::OrigType {
   Sent = 0,
   /// >= SendRequestResult::FlowControl 表示流量管制.
   /// 可透過 ToFlowControlInterval(res); 取得: 還要等候多久才解除管制.
   FlowControl = 1,
   /// 線路忙碌中, 例如: 每次只能送1筆, 必須等收到結果後才能送下一筆.
   Busy = -1,
   /// 線路已斷線無法再送單.
   Broken = -2,
};
inline TimeInterval ToFlowControlInterval(SendRequestResult r) {
   assert(r >= SendRequestResult::FlowControl);
   return TimeInterval::Make<6>(static_cast<TimeInterval::OrigType>(r));
}
inline SendRequestResult ToFlowControlResult(TimeInterval ti) {
   assert(static_cast<SendRequestResult>(ti.GetOrigValue()) >= SendRequestResult::FlowControl);
   return static_cast<SendRequestResult>(ti.GetOrigValue());
}

/// \ingroup fmkt
/// 交易連線基底.
class fon9_API TradingLine {
   fon9_NON_COPY_NON_MOVE(TradingLine);
public:
   TradingLine() = default;

   virtual ~TradingLine();

   /// 設計衍生者請注意:
   /// 透過 TradingLineManager 來的下單要求, 必定已經鎖住「可用線路表」,
   /// 因此不可再呼叫 TradingLineManager 的相關函式, 會造成死結!
   virtual SendRequestResult SendRequest(TradingRequest& req) = 0;
};

fon9_WARN_DISABLE_PADDING;
/// \ingroup fmkt
/// 交易連線管理員基底.
/// - 負責尋找可下單的線路送出下單要求.
/// - 負責等候流量管制時間, 時間到解除管制時, 透過 OnNewTradingLineReady() 通知衍生者.
class fon9_API TradingLineManager {
   fon9_NON_COPY_NON_MOVE(TradingLineManager);
   /// 可送出下單要求的連線. 流量管制時不移除.
   using TradingLinesImpl = std::vector<TradingLine*>;
   using TradingLines = MustLock<TradingLinesImpl>;
   TradingLines ReadyLines_;
   unsigned     LineIndex_{0};

public:
   using Locker = TradingLines::Locker;

   virtual ~TradingLineManager();

   /// 當 src 進入可下單狀態時的通知:
   /// - 連線成功後, 進入可下單狀態.
   /// - 從忙碌狀態, 進入可下單狀態.
   void OnTradingLineReady(TradingLine& src);

   /// 當 src 斷線時的通知.
   /// 不包含: 流量管制, 線路忙碌.
   void OnTradingLineBroken(TradingLine& src);

   SendRequestResult SendRequest(TradingRequest& req, const Locker* tlines = nullptr) {
      if (tlines)
         return this->SendRequest(req, *tlines);
      return this->SendRequest(req, TradingLines::Locker{this->ReadyLines_});
   }

protected:
   /// 當有新的可交易線路時的通知, 預設: do nothing.
   /// 若 src == nullptr 表示流量管制解除.
   virtual void OnNewTradingLineReady(TradingLine* src, const Locker&);

private:
   SendRequestResult SendRequest(TradingRequest& req, const Locker& tlines);

   struct FlowControlTimer : public DataMemberTimer {
      fon9_NON_COPY_NON_MOVE(FlowControlTimer);
      FlowControlTimer() = default;
      virtual void EmitOnTimer(TimeStamp now) override;
   };
   FlowControlTimer FlowControlTimer_;
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_fmkt_Trading_hpp__
