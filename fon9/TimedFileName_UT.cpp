// \file fon9/TimedFileName_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "fon9/TestTools.hpp"
#include "fon9/TimedFileName.hpp"

using TimeScale = fon9::TimedFileName::TimeScale;
using StrView = fon9::StrView;

void TestTimedFileName(fon9::TimedFileName& tfn, const char* tstr, const StrView expected) {
   bool chk = tfn.CheckTime(fon9::StrTo(fon9::StrView_cstr(tstr), fon9::TimeStamp{}));
   std::cout << "tm=" << tstr << "|fn='" << tfn.GetFileName() << "'";
   if (expected.empty()) { //期望 tfn.CheckTime() 不變.
      if (!chk) {
         std::cout << std::endl;
         return;
      }
      std::cout << "|***** not expect, ChekcTime() changed *****" << std::endl;
      abort();
   }
   if (expected == &tfn.GetFileName()) {
      std::cout << std::endl;
      return;
   }
   std::cout << "|FileName=" << tfn.GetFileName() << "|***** not expect *****" << std::endl;
   abort();
}

void TestTimedFileName(TimeScale tscale) {
   fon9::TimedFileName tfn("./log/{0:f-t}.log", tscale);
   TestTimedFileName(tfn, "2016/09/04-18:31:30", "./log/20160904-183130.log");
   TestTimedFileName(tfn, "2016/09/04-18:31:31", tscale > TimeScale::Second ? StrView{} : StrView{"./log/20160904-183131.log"});
   TestTimedFileName(tfn, "2016/09/04-18:32:00", tscale > TimeScale::Minute ? StrView{} : StrView{"./log/20160904-183200.log"});
   TestTimedFileName(tfn, "2016/09/04-19:32:00", tscale > TimeScale::Hour   ? StrView{} : StrView{"./log/20160904-193200.log"});
   TestTimedFileName(tfn, "2016/09/05-19:32:00", tscale > TimeScale::Day    ? StrView{} : StrView{"./log/20160905-193200.log"});
   TestTimedFileName(tfn, "2016/10/05-19:32:00", tscale > TimeScale::Month  ? StrView{} : StrView{"./log/20161005-193200.log"});
   TestTimedFileName(tfn, "2017/10/05-19:32:00", StrView{"./log/20171005-193200.log"});
   if (tfn.AddSeqNo()) {
      std::cout << "AddSeqNo() fail!" << std::endl;
      abort();
   }
   // 時間倒回 n 秒, 檔名不變.
   TestTimedFileName(tfn, "2017/10/05-19:31:20", StrView{});
   TestTimedFileName(tfn, "2017/10/05-19:31:01", StrView{});
   TestTimedFileName(tfn, "2017/10/05-19:31:00", tscale > TimeScale::Minute ? StrView{} : StrView{"./log/20171005-193100.log"});
   TestTimedFileName(tfn, "2017/10/05-18:31:00", tscale > TimeScale::Hour   ? StrView{} : StrView{"./log/20171005-183100.log"});
   TestTimedFileName(tfn, "2017/10/04-18:31:00", tscale > TimeScale::Day    ? StrView{} : StrView{"./log/20171004-183100.log"});
   TestTimedFileName(tfn, "2017/09/04-18:31:00", tscale > TimeScale::Month  ? StrView{} : StrView{"./log/20170904-183100.log"});
   TestTimedFileName(tfn, "2016/09/04-18:31:00", tscale > TimeScale::Year   ? StrView{} : StrView{"./log/20160904-183100.log"});
}
void TestTimedFileName_SeqNo() {
   fon9::TimedFileName tfn("./log/{0:f-H}.{1:04}.log", TimeScale::Minute);
   TestTimedFileName(tfn, "20160904-101234", StrView{"./log/20160904-10.0000.log"});
   TestTimedFileName(tfn, "20160904-101254", StrView{});
   // Minute改變, 但是fmt只有 YYYYMMDD-HH, 所以自動增加序號.
   TestTimedFileName(tfn, "20160904-101300", StrView{"./log/20160904-10.0001.log"});
   // Hour改變, 序號歸0.
   TestTimedFileName(tfn, "20160904-111300", StrView{"./log/20160904-11.0000.log"});
   // 有序號參數,加序號檔名會改變.
   if (tfn.AddSeqNo()) {
      if (tfn.GetFileName() != "./log/20160904-11.0001.log") {
         std::cout << "AddSeqNo() FileName not expect!" << std::endl;
         abort();
      }
      std::cout << "After AddSeqNo()  |fn='" << tfn.GetFileName() << "'" << std::endl;
   }
   else {
      std::cout << "AddSeqNo() fail!" << std::endl;
      abort();
   }
}

int main() {
#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

   fon9::AutoPrintTestInfo utinfo("TimedFileName");
   std::cout << "--- TimeScale::Second ---\n";
   TestTimedFileName(TimeScale::Second);
   std::cout << "--- TimeScale::Minute ---\n";
   TestTimedFileName(TimeScale::Minute);
   std::cout << "--- TimeScale::Hour ---\n";
   TestTimedFileName(TimeScale::Hour);
   std::cout << "--- TimeScale::Day ---\n";
   TestTimedFileName(TimeScale::Day);
   std::cout << "--- TimeScale::Month ---\n";
   TestTimedFileName(TimeScale::Month);
   std::cout << "--- TimeScale::Year ---\n";
   TestTimedFileName(TimeScale::Year);

   std::cout << "--- SeqNo ---\n";
   TestTimedFileName_SeqNo();

   std::cout << "--- TimeZone+8 ---\n";
   fon9::TimedFileName tfn("./log/{0:f-H+8}.{1:04}.log", TimeScale::Minute);
   TestTimedFileName(tfn, "20160904-101234", "./log/20160904-18.0000.log");
   TestTimedFileName(tfn, "20160904-111234", "./log/20160904-19.0000.log");
   TestTimedFileName(tfn, "20160904-201234", "./log/20160905-04.0000.log");
}
