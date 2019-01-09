// \file f9tws/TradingLineFix.cpp
// \author fonwinz@gmail.com
#include "f9tws/TradingLineFix.hpp"
#include "fon9/fix/FixAdminDef.hpp"
#include "fon9/FilePath.hpp"

namespace f9tws {

// 台灣證交所規定 10 秒.
constexpr uint32_t   kHeartBtInt = 10;

//--------------------------------------------------------------------------//

f9tws_API std::string TwsFixArgParser(TradingLineFixArgs& args, fon9::StrView cfg) {
   /// cfg = "BrkId=|SocketId=|Pass=|Fc=每秒最多筆數" 每個欄位都必須提供.
   args.BrkId_.Clear(' ');
   args.SocketId_.Clear(' ');
   args.PassCode_ = static_cast<uint16_t>(-1);
   args.MaxRequestsPerSec_ = static_cast<uint32_t>(-1);
   fon9::StrView tag, value;
   while (fon9::StrFetchTagValue(cfg, tag, value)) {
      if (tag == "BrkId")
         args.BrkId_.AssignFrom(value);
      else if (tag == "SocketId" || tag == "PvcId")
         args.SocketId_.AssignFrom(value);
      else {
         if (tag == "Pass")
            args.PassCode_ = fon9::StrTo(&value, args.PassCode_);
         else if (tag == "Fc")
            args.MaxRequestsPerSec_ = fon9::StrTo(&value, args.MaxRequestsPerSec_);
         else
            return tag.ToString("Unknown tag: ");
         if (!StrTrim(&value).empty())
            return tag.ToString() + value.ToString("Unknown value: ");
      }
   }
   if (fon9::isspace(args.BrkId_.Chars_[0]))
      return "Unknown BrkId";
   if (fon9::isspace(args.SocketId_.Chars_[0]))
      return "Unknown SocketId";
   if (static_cast<int16_t>(args.PassCode_) < 0)
      return "Unknown Pass";
   if (static_cast<int32_t>(args.MaxRequestsPerSec_) < 0)
      return "Unknown Fc";
   return std::string{};
}

//--------------------------------------------------------------------------//

static f9fix::CompIDs MakeCompIDs(const TradingLineFixArgs& args) {
   static_assert(f9fmkt_TradingMarket_TwSEC == 'T' && f9fmkt_TradingMarket_TwOTC == 'O',
                "f9fmkt_TradingMarket_TwSEC == 'T' && f9fmkt_TradingMarket_TwOTC == 'O'");
   char  senderCompID[7];
   senderCompID[0] = args.Market_;
   memcpy(senderCompID + 1, args.BrkId_.data(), 4);
   memcpy(senderCompID + 5, args.SocketId_.data(), 2);
   return f9fix::CompIDs(fon9::StrView_all(senderCompID), nullptr,
            args.Market_ == f9fmkt_TradingMarket_TwSEC ? fon9::StrView{"XTAI"} : fon9::StrView{"ROCO"}, nullptr);
}

f9tws_API std::string MakeTradingLineFixSender(const TradingLineFixArgs& args, fon9::StrView recPath, f9fix::IoFixSenderSP& out) {
   /// - 上市 retval->Initialize(recPath + "FIX44_XTAI_BrkId_SocketId.log");
   /// - 上櫃 retval->Initialize(recPath + "FIX44_ROCO_BrkId_SocketId.log");
   f9fix::IoFixSenderSP fixSender{new f9fix::IoFixSender{f9fix_BEGIN_HEADER_V44, MakeCompIDs(args)}};
   std::string          fileName = fon9::FilePath::AppendPathTail(recPath);
   fileName += "FIX44_";
   fixSender->CompIDs_.Target_.CompID_.AppendTo(fileName); // "XTAI" or "ROCO"
   fileName.push_back('_');
   fixSender->CompIDs_.Sender_.CompID_.AppendTo(fileName); // BrkId
   fileName.push_back('_');
   fileName.append(args.SocketId_.data(), args.SocketId_.size());
   fileName.append(".log");
   auto res = fixSender->GetFixRecorder().Initialize(fileName);
   if (res.IsError())
      return fon9::RevPrintTo<std::string>("MakeTradingLineFixSender|fn=", fileName, '|', res);
   out = std::move(fixSender);
   return std::string{};
}

//--------------------------------------------------------------------------//

TradingLineFix::TradingLineFix(TradingLineFixMgr&        mgr,
                               const f9fix::FixConfig&   fixcfg,
                               const TradingLineFixArgs& lineargs,
                               f9fix::IoFixSenderSP&&    fixSender)
   : base(mgr, fixcfg)
   , RawAppendNo_{static_cast<unsigned>(fon9::UtcNow().GetDecPart() / 1000)}
   , LineArgs_(lineargs)
   , FixSender_{std::move(fixSender)} {
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
