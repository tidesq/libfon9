// \file f9tws/TradingLineMgr.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_TradingLineMgr_hpp__
#define __f9tws_TradingLineMgr_hpp__
#include "f9tws/TradingLineFix.hpp"
#include "fon9/fmkt/Trading.hpp"
#include "fon9/framework/NamedIoManager.hpp"

namespace f9tws {

fon9_WARN_DISABLE_PADDING;
class f9tws_API TradingLineMgr : public fon9::NamedIoManager
                               , public f9fmkt::TradingLineManager
                               , public TradingLineFixMgr {
   fon9_NON_COPY_NON_MOVE(TradingLineMgr);
   using baseIo = fon9::NamedIoManager;
   using baseFix = TradingLineFixMgr;

public:
   const f9fmkt_TradingMarket Market_;

   TradingLineMgr(f9fmkt_TradingMarket mkt, const fon9::IoManagerArgs& ioargs)
      : baseIo(ioargs)
      , Market_(mkt) {
   }
   void OnFixSessionApReady(f9fix::IoFixSession& fixses) override;
   void OnFixSessionDisconnected(f9fix::IoFixSession& fixses, f9fix::FixSenderSP&& fixSender) override;
};
fon9_WARN_POP;

} // namespaces
#endif//__f9tws_TradingLineMgr_hpp__
