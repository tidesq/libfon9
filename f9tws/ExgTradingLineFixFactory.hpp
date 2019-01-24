// \file f9tws/ExgTradingLineFixFactory.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgTradingLineFixFactory_hpp__
#define __f9tws_ExgTradingLineFixFactory_hpp__
#include "f9tws/ExgTradingLineFix.hpp"
#include "f9tws/ExgTradingLineMgr.hpp"
#include "fon9/framework/IoFactory.hpp"

namespace f9tws {

class f9tws_API ExgTradingLineFixFactory : public fon9::SessionFactory {
   fon9_NON_COPY_NON_MOVE(ExgTradingLineFixFactory);
   using base = fon9::SessionFactory;
protected:
   f9fix::FixConfig  FixConfig_;
   std::string       FixLogPathFmt_;

   virtual fon9::io::SessionSP CreateTradingLine(ExgTradingLineMgr&           lineMgr,
                                                 const ExgTradingLineFixArgs& args,
                                                 f9fix::IoFixSenderSP         fixSender) = 0;
public:
   /// 衍生者必須自行處理相關訊息:
   /// this->FixConfig_.Fetch(f9fix_kMSGTYPE_SessionReject).FixRejectHandler_ = &OnFixReject;
   /// this->FixConfig_.Fetch(f9fix_kMSGTYPE_BusinessReject).FixRejectHandler_ = &OnFixReject;
   /// this->FixConfig_.Fetch(f9fix_kMSGTYPE_ExecutionReport).FixMsgHandler_ = &OnFixExecutionReport;
   /// this->FixConfig_.Fetch(f9fix_kMSGTYPE_OrderCancelReject).FixMsgHandler_ = &OnFixCancelReject;
   ExgTradingLineFixFactory(std::string fixLogPathFmt, Named&& name);

   /// mgr 必須是 f9tws::ExgTradingLineMgr
   fon9::io::SessionSP CreateSession(fon9::IoManager& mgr, const fon9::IoConfigItem& cfg, std::string& errReason) override;
   /// 不支援.
   fon9::io::SessionServerSP CreateSessionServer(fon9::IoManager& mgr, const fon9::IoConfigItem& cfg, std::string& errReason) override;
};

} // namespaces
#endif//__f9tws_ExgTradingLineFixFactory_hpp__
