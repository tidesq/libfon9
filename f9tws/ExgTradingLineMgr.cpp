// \file f9tws/ExgTradingLineMgr.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgTradingLineMgr.hpp"

namespace f9tws {

void ExgTradingLineMgr::OnFixSessionApReady(f9fix::IoFixSession& fixses) {
   if (auto line = dynamic_cast<ExgTradingLineFix*>(&fixses)) {
      baseFix::OnFixSessionApReady(fixses);
      this->OnTradingLineReady(*line);
   }
   else {
      this->OnSession_StateUpdated(*fixses.GetDevice(), "Unknown FIX.Ready", fon9::LogLevel::Error);
   }
}
void ExgTradingLineMgr::OnFixSessionDisconnected(f9fix::IoFixSession& fixses, f9fix::FixSenderSP&& fixSender) {
   if (auto line = dynamic_cast<ExgTradingLineFix*>(&fixses))
      this->OnTradingLineBroken(*line);
   baseFix::OnFixSessionDisconnected(fixses, std::move(fixSender));
}

} // namespaces
