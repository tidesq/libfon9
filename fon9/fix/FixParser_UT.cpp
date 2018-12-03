// \file fon9/fix/FixParser_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "fon9/TestTools.hpp"
#include "fon9/fix/FixParser.hpp"
#include "fon9/fix/FixBuilder.hpp"
#include "fon9/Timer.hpp"

namespace f9fix = fon9::fix;

fon9_WARN_DISABLE_PADDING;
struct FldValue {
   f9fix::FixTag Tag_;
   fon9::StrView Value_;
   unsigned      Index_;

   FldValue(f9fix::FixTag tag, const fon9::StrView& value, unsigned index = 0)
      : Tag_{tag}, Value_{value}, Index_{index} {
   }
};
fon9_WARN_POP;

//--------------------------------------------------------------------------//

//void PrintFixFieldList(const f9fix::FixParser& fixpr) {
//   for (auto fld : fixpr)
//      std::cout << '[' << fld->Tag_ << "]=[" << fld->Value_.ToString() << ']' << std::endl;
//}

f9fix::FixParser::Result TestFixParser(const char* testName,
                                       f9fix::FixParser& fixpr,
                                       fon9::StrView fixmsg,
                                       const FldValue* values,
                                       size_t valueCount,
                                       f9fix::FixParser::Until until = f9fix::FixParser::Until::FullMessage) {
   std::cout << "[TEST ] " << testName;
   const fon9::StrView origmsg{fixmsg};
   auto const msgsz = fixmsg.size();
   auto const res = fixpr.Parse(fixmsg, until);
   if (static_cast<int>(res) != static_cast<int>(msgsz)) {
      std::cout << "FixParser::Parse() error|expect=" << msgsz << "|result=" << static_cast<int>(res)
         << "\r" "[ERROR]" << std::endl;
      abort();
   }
   if (fixpr.count() != valueCount) {
      std::cout << "field count error|expect=" << valueCount << "|result=" << fixpr.count() << std::endl;
      abort();
   }
   size_t idx = 0;
   for (auto fld : fixpr) {
      if (values->Tag_ != fld->Tag_) {
         std::cout << "field Tag# error|index=" << idx << "|expect=" << values->Tag_ << "|result=" << fld->Tag_ << std::endl;
         abort();
      }
      if (values->Value_ != fixpr.GetValue(*fld, values->Index_)) {
         std::cout << "field Value error|index=" << idx << "|tag=" << fld->Tag_
            << "|expect=" << values->Value_.ToString() << "|result=" << fld->Value_.ToString() << std::endl;
         abort();
      }
      ++idx;
      ++values;
   }
   // 測試 FixBuilder.
   if (until == f9fix::FixParser::Until::FullMessage) {
      f9fix::FixBuilder fbuf;
      while (idx) {
         --idx;
         --values;
         fon9::RevPrint(fbuf.GetBuffer(), f9fix_kCHAR_SPL, values->Tag_, '=', values->Value_);
      }
      auto bstr = fon9::BufferTo<std::string>(fbuf.Final(ToStrView(fixpr.GetExpectHeader())));
      if (origmsg != fon9::ToStrView(bstr)) {
         std::cout << "FixBuilder error." "\r" "[ERROR]" << std::endl;
         abort();
      }
   }
   std::cout << "\r" "[OK   ]" << std::endl;
   return res;
}

int main(int argc, char** args) {
   (void)argc; (void)args;

#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

   fon9::AutoPrintTestInfo utinfo{"Symb"};
   fon9::GetDefaultTimerThread();
   std::this_thread::sleep_for(std::chrono::milliseconds{10});

   f9fix::FixParser   fixpr;
   #define _   f9fix_kCSTR_SPL

   // 一般訊息.
   const FldValue vs1[] = {{35,"A"},{56,"Client"},{49,"Server"},{34,"1"},{52,"20170426-00:49:26.625"},{108,"3000"},{98,"0"}};
   TestFixParser("Simple FIX Message.", fixpr,
                 "8=FIX.4.4" _ "9=69" _ "35=A" _ "56=Client" _ "49=Server" _ "34=1" _ "52=20170426-00:49:26.625" _ "108=3000" _ "98=0" _ "10=067" _,
                 vs1, fon9::numofele(vs1));
   // Test: RawDataLength + RawData(and test byte: '\0' '\x01')
   const FldValue vs2[] = {{35,"A"},{49,"Client"},{56,"Server"},{34,"2"},{52,"20170426-08:49:28"},{95,"32"},{96,"TraderID=fonwin\0\x01Password=fonwin"},{108,"3000"}};
   TestFixParser("RawDataLength+RawData.", fixpr,
                 "8=FIX.4.4" _ "9=102" _ "35=A" _ "49=Client" _ "56=Server" _ "34=2" _ "52=20170426-08:49:28" _ "95=32" _ "96=TraderID=fonwin\0\x01Password=fonwin" _ "108=3000" _ "10=101" _,
                 vs2, fon9::numofele(vs2));
   if (fixpr.GetField(98)) { // vs1,vs2使用相同fixpr. #98僅存在於vs1, 所以解析 vs2 之後, #98 應該不存在.
      std::cout << "Unexpected field #98." << std::endl;
      abort();
   }
   // Test: RawData + RawDataLength
   const FldValue vs3[] = {{35,"A"},{49,"Client"},{56,"Server"},{34,"3"},{52,"20170426-08:49:28"},{96,"TraderID=fonwinPassword=fonwin"},{95,"30"},{98,"0"}};
   TestFixParser("RawData+RawDataLength(don't care).", fixpr,
                 "8=FIX.4.4" _ "9=96" _ "35=A" _ "49=Client" _ "56=Server" _ "34=3" _ "52=20170426-08:49:28" _ "96=TraderID=fonwinPassword=fonwin" _ "95=30" _ "98=0" _ "10=132" _,
                 vs3, fon9::numofele(vs3));
   if (fixpr.GetField(108)) {
      std::cout << "Unexpected field #108." << std::endl;
      abort();
   }

   // Test: tag 有無效(非數字)字元.
   fon9::StrView fixmsg = fon9::StrView("8=FIX.4.4" _ "9=70" _ "3x5=A" _ "56=Client" _ "49=Server" _ "34=4" _ "52=20170426-00:49:26.625" _ "108=3000" _ "98=0" _ "10=182" _);
   if (fixpr.Parse(fixmsg) != f9fix::FixParser::EFormat || fixmsg.Get1st() != 'x') { // x5=A...
      std::cout << "Unexpected error pos." << fixmsg.begin() << std::endl;
      abort();
   }

   // Test Parse until.
   const FldValue vs5[] = {{35,"A"},{49,"Client"},{56,"Server"},{34,"5"}};
   TestFixParser("Until:MsgSeqNum+MsgType.", fixpr,
                 "8=FIX.4.4" _ "9=105" _ "35=A" _ "49=Client" _ "56=Server" _ "34=5" _ "52=20170426-08:49:28" _ "108=3000" _ "98=0" _ "95=30" _ "96=TraderID=fonwinPassword=fonwin" _ "10=071" _,
                 vs5, fon9::numofele(vs5),
                 f9fix::FixParser::Until::MsgSeqNum | f9fix::FixParser::Until::MsgType);

   // Test: 重複欄位.
   const FldValue vs6[] = {{35,"A"},{49,"Client"},{56,"Server"},{34,"6"},{52,"20170426-08:49:28"},{108,"3000"},{98,"0"},
                           {99,"A"},{99,"B",1},{99,"C",2},{99,"D",3},{99,"E",4}};
   TestFixParser("Repeat fields.", fixpr,
                 "8=FIX.4.4" _ "9=90" _ "35=A" _ "49=Client" _ "56=Server" _ "34=6" _ "52=20170426-08:49:28" _ "108=3000" _ "98=0" _
                 "99=A" _ "99=B" _ "99=C" _ "99=D" _ "99=E" _ "10=064" _,
                 vs6, fon9::numofele(vs6));
   // Test: 超過可重複次數.
   fixmsg = fon9::StrView{"8=FIX.4.4" _ "9=95" _ "35=A" _ "49=Client" _ "56=Server" _ "34=6" _ "52=20170426-08:49:28" _ "108=3000" _ "98=0" _
                          "99=A" _ "99=B" _ "99=C" _ "99=D" _ "99=E" _ "99=F" _ "10=059" _};
   if (fixpr.Parse(fixmsg) != f9fix::FixParser::EDupField) {
      std::cout << "Unexpected EDupField." << std::endl;
      abort();
   }
   // Test: 2組重複欄位.
   const FldValue vs7[] = {{35,"A"},{49,"Client"},{56,"Server"},{34,"7"},{52,"20170426-08:49:28"},{108,"3000"},
                           {98,"0"},{98,"1",1},{98,"2",2},{98,"3",3},{98,"4",4},
                           {99,"A"},{99,"B",1},{99,"C",2},{99,"D",3},{99,"E",4}};
   TestFixParser("Repeat #98, #99.", fixpr,
                 "8=FIX.4.4" _ "9=110" _ "35=A" _ "49=Client" _ "56=Server" _ "34=7" _ "52=20170426-08:49:28" _ "108=3000" _
                 "98=0" _ "98=1" _ "98=2" _ "98=3" _ "98=4" _
                 "99=A" _ "99=B" _ "99=C" _ "99=D" _ "99=E" _ "10=240" _,
                 vs7, fon9::numofele(vs7));
}
