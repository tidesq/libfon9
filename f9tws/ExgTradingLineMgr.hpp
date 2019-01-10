// \file f9tws/ExgExgTradingLineMgr.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgTradingLineMgr_hpp__
#define __f9tws_ExgTradingLineMgr_hpp__
#include "f9tws/ExgTradingLineFix.hpp"
#include "fon9/framework/NamedIoManager.hpp"

namespace f9tws {

fon9_WARN_DISABLE_PADDING;
class f9tws_API ExgTradingLineMgr : public fon9::NamedIoManager       // 負責管理: 通訊設定、狀態log、通訊物件的生死.
                                  , public f9fmkt::TradingLineManager // 負責管理: 可用線路、下單時尋找適當線路.
                                  , public TradingLineFixMgr {        // 負責處理: Fix線路可用時通知 f9fmkt::TradingLineManager
   fon9_NON_COPY_NON_MOVE(ExgTradingLineMgr);
   using baseIo = fon9::NamedIoManager;
   using baseFix = TradingLineFixMgr;

public:
   const f9fmkt_TradingMarket Market_;

   ExgTradingLineMgr(f9fmkt_TradingMarket mkt, const fon9::IoManagerArgs& ioargs)
      : baseIo(ioargs)
      , Market_(mkt) {
   }
   void OnFixSessionApReady(f9fix::IoFixSession& fixses) override;
   void OnFixSessionDisconnected(f9fix::IoFixSession& fixses, f9fix::FixSenderSP&& fixSender) override;
};
fon9_WARN_POP;

} // namespaces
#endif//__f9tws_ExgTradingLineMgr_hpp__
