/// \file fon9/fmkt/Trading.cpp
/// \author fonwinz@gmail.com
#include "fon9/fmkt/Trading.hpp"

namespace fon9 { namespace fmkt {

TradingRequest::~TradingRequest() {
}

TradingLine::~TradingLine() {
}

TradingLineManager::~TradingLineManager() {
   this->FlowControlTimer_.StopAndWait();
}
void TradingLineManager::OnNewTradingLineReady(TradingLine*, const Locker&) {
}
void TradingLineManager::OnTradingLineReady(TradingLine& src) {
   TradingLines::Locker tlines{this->ReadyLines_};
   auto ifind = std::find(tlines->begin(), tlines->end(), &src);
   if (ifind == tlines->end())
      tlines->push_back(&src);
   this->OnNewTradingLineReady(&src, tlines);
}
void TradingLineManager::OnTradingLineBroken(TradingLine& src) {
   TradingLines::Locker tlines{this->ReadyLines_};
   auto ifind = std::find(tlines->begin(), tlines->end(), &src);
   if (ifind != tlines->end())
      tlines->erase(ifind);
}
SendRequestResult TradingLineManager::SendRequest(TradingRequest& req, const Locker& tlines) {
   if (size_t sz = tlines->size()) {
      SendRequestResult resFlowControl{SendRequestResult::Busy};
      for (size_t L = 0; L < sz; ++L) {
         ++this->LineIndex_;
      __RETRY_SAME_INDEX:
         if (this->LineIndex_ >= sz)
            this->LineIndex_ = 0;
         SendRequestResult res = (*tlines)[this->LineIndex_]->SendRequest(req);
         if (fon9_LIKELY(res == SendRequestResult::Sent))
            return res;
         if (fon9_LIKELY(res >= SendRequestResult::FlowControl)) {
            if (resFlowControl < SendRequestResult::FlowControl || res < resFlowControl)
               resFlowControl = res;
         }
         else if (fon9_LIKELY(res != SendRequestResult::Busy)) {
            assert(res == SendRequestResult::Broken);
            tlines->erase(tlines->begin() + this->LineIndex_);
            if (L < --sz)
               goto __RETRY_SAME_INDEX;
            if (sz == 0)
               return SendRequestResult::Broken;
            break;
         }
      }
      if (resFlowControl >= SendRequestResult::FlowControl)
         this->FlowControlTimer_.RunAfter(ToFlowControlInterval(resFlowControl));
      return resFlowControl;
   }
   return SendRequestResult::Broken;
}

void TradingLineManager::FlowControlTimer::EmitOnTimer(TimeStamp now) {
   (void)now;
   TradingLineManager&  rmgr = ContainerOf(*this, &TradingLineManager::FlowControlTimer_);
   TradingLines::Locker tlines{rmgr.ReadyLines_};
   if (!tlines->empty())
      rmgr.OnNewTradingLineReady(nullptr, tlines);
}

} } // namespaces
