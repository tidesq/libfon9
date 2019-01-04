// \file f9tws/TradingLineFix.hpp
//
// TSE/OTC FIX 下單線路.
//
// \author fonwinz@gmail.com
#ifndef __f9tws_TradingLineFix_hpp__
#define __f9tws_TradingLineFix_hpp__
#include "f9tws/Config.h"
#include "fon9/fix/IoFixSession.hpp"
#include "fon9/fix/IoFixSender.hpp"
#include "fon9/fmkt/FmktTypes.hpp"
#include "fon9/CharAry.hpp"

namespace f9tws {
namespace f9fix = fon9::fix;
namespace f9fmkt = fon9::fmkt;

fon9_WARN_DISABLE_PADDING;
struct TradingLineFixArgs {
   fon9::CharAry<4>     BrkId_;
   fon9::CharAry<2>     SocketId_;
   uint16_t             PassCode_;
   /// 流量管制: 每秒最多筆數. 0=不限制.
   uint32_t             MaxRequestsPerSec_;
   f9fmkt_TradingMarket Market_;
};

class f9tws_API TradingLineFix : public f9fix::IoFixSession {
   fon9_NON_COPY_NON_MOVE(TradingLineFix);
   using base = f9fix::IoFixSession;
   unsigned RawAppendNo_;

protected:
   /// 連線成功, 在此主動送出 Logon 訊息.
   void OnFixSessionConnected() override;
   f9fix::FixSenderSP OnFixSessionDisconnected(const fon9::StrView& info) override;

public:
   const f9fix::IoFixSenderSP FixSender_;
   const TradingLineFixArgs   LineArgs_;

   TradingLineFix(f9fix::IoFixManager& mgr, const f9fix::FixConfig& fixcfg, const TradingLineFixArgs& lineargs);
};
fon9_WARN_POP;

} // namespaces
#endif//__f9tws_TradingLineFix_hpp__
