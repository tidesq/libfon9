// \file f9tws/TradingLineFix.cpp
// \author fonwinz@gmail.com
#include "f9tws/TradingLineFix.hpp"
#include "fon9/fix/FixAdminDef.hpp"

namespace f9tws {

// 台灣證交所規定 10 秒.
constexpr uint32_t   kHeartBtInt = 10;

static f9fix::CompIDs MakeCompIDs(const TradingLineFixArgs& lineargs) {
   static_assert(f9fmkt_TradingMarket_TwSEC == 'T' && f9fmkt_TradingMarket_TwOTC == 'O',
                "f9fmkt_TradingMarket_TwSEC == 'T' && f9fmkt_TradingMarket_TwOTC == 'O'");
   char  senderCompID[7];
   senderCompID[0] = lineargs.Market_;
   memcpy(senderCompID + 1, lineargs.BrkId_.data(), 4);
   memcpy(senderCompID + 5, lineargs.SocketId_.data(), 2);
   return f9fix::CompIDs(fon9::StrView_all(senderCompID), nullptr,
     lineargs.Market_ == f9fmkt_TradingMarket_TwSEC ? fon9::StrView{"XTAI"} : fon9::StrView{"ROCO"}, nullptr);
}

TradingLineFix::TradingLineFix(f9fix::IoFixManager& mgr, const f9fix::FixConfig& fixcfg, const TradingLineFixArgs& lineargs)
   : base(mgr, fixcfg)
   , RawAppendNo_{static_cast<unsigned>(fon9::UtcNow().GetDecPart() / 1000)}
   , FixSender_{new f9fix::IoFixSender{f9fix_BEGIN_HEADER_V44, MakeCompIDs(lineargs)}}
   , LineArgs_(lineargs) {
}
void TradingLineFix::OnFixSessionConnected() {
   base::OnFixSessionConnected();
   this->FixSender_->OnFixSessionConnected(this->GetDevice());
   f9fix::FixBuilder fixb;
   // 由證券商每次隨機產生一組三位數字。 001 <= APPEND-NO <= 999。此值不能與前五次登入使用相同之值。
   uint32_t app = (this->RawAppendNo_ + 1) % 1000;
   if (app == 0)
      app = 1;
   this->RawAppendNo_ = app;
   // KEY-VALUE = (APPEND-NO * PASSWORD)取千與百二位數字。
   // APPEND-NO + KEY-VALUE: 3 digits + 2 digits
   #define kRawDataLength     5
   app = (app * 100) + (((app * this->LineArgs_.PassCode_) / 100) % 100);
   auto pOutRawData = fixb.GetBuffer().AllocPrefix(kRawDataLength);
   fixb.GetBuffer().SetPrefixUsed(fon9::Pic9ToStrRev<kRawDataLength>(pOutRawData, app));
   fon9::RevPrint(fixb.GetBuffer(), f9fix_kFLD_EncryptMethod_None
                  f9fix_SPLTAGEQ(RawDataLength) fon9_CTXTOCSTR(kRawDataLength)
                  f9fix_SPLTAGEQ(RawData));
   this->SendLogon(this->FixSender_, kHeartBtInt, std::move(fixb));
}
f9fix::FixSenderSP TradingLineFix::OnFixSessionDisconnected(const fon9::StrView& info) {
   this->FixSender_->OnFixSessionDisconnected();
   return base::OnFixSessionDisconnected(info);
}

} // namespaces
