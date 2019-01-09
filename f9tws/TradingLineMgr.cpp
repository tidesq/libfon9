// \file f9tws/TradingLineMgr.cpp
// \author fonwinz@gmail.com
#include "f9tws/TradingLineMgr.hpp"

namespace f9tws {

void TradingLineMgr::OnFixSessionApReady(f9fix::IoFixSession& fixses) {
   if (auto line = dynamic_cast<TradingLineFix*>(&fixses)) {
      baseFix::OnFixSessionApReady(fixses);
      this->OnTradingLineReady(*line);
   }
   else {
      this->GetIoManager().OnSession_StateUpdated(*fixses.GetDevice(), "Unknown FIX.Ready", fon9::LogLevel::Error);
   }
}
void TradingLineMgr::OnFixSessionDisconnected(f9fix::IoFixSession& fixses, f9fix::FixSenderSP&& fixSender) {
   if (auto line = dynamic_cast<TradingLineFix*>(&fixses))
      this->OnTradingLineBroken(*line);
   baseFix::OnFixSessionDisconnected(fixses, std::move(fixSender));
}

} // namespaces
