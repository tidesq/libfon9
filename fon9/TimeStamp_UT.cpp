// \file fon9/TimeStamp_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "fon9/TimeStamp.hpp"
#include "fon9/TestTools.hpp"

#ifdef fon9_WINDOWS
fon9_BEFORE_INCLUDE_STD;
#include <WinSock2.h>// struct timeval;
fon9_AFTER_INCLUDE_STD;

/// FILETIME of Jan 1 1970 00:00:00.
static const unsigned __int64 epoch = ((unsigned __int64)116444736000000000ULL);

/// 不支援 tzp
static int gettimeofday(struct timeval* tp, struct timezone* /*tzp*/) {
   FILETIME       file_time;
   SYSTEMTIME     system_time;
   ULARGE_INTEGER ularge;

   GetSystemTime(&system_time);
   SystemTimeToFileTime(&system_time, &file_time);
   ularge.LowPart = file_time.dwLowDateTime;
   ularge.HighPart = file_time.dwHighDateTime;

   tp->tv_sec = (long)((ularge.QuadPart - epoch) / 10000000L);
   tp->tv_usec = (long)(system_time.wMilliseconds * 1000);

   return 0;
}
static const unsigned   kTimes = 1000 * 1000;
#else
#include <sys/time.h>
static const unsigned   kTimes = 1000 * 1000;
#endif

//--------------------------------------------------------------------------//

template <typename TimeT>
void CheckToStrRev(TimeT ts, char* (*fnToStrRev)(char*, TimeT), const char* fnName, const char* expectResult) {
   fon9::NumOutBuf nbuf;
   nbuf.SetEOS();
   char* pout = fnToStrRev(nbuf.end(), ts);
   if (strcmp(pout, expectResult) != 0) {
      std::cout << "[ERROR] " << fnName << ":'" << pout << "'|expect='" << expectResult << '\'' << std::endl;
      abort();
   }
   std::cout << "[OK   ] " << fnName << ":'" << pout << '\'' << std::endl;
}
void TimeInterval_TestToStrRev() {
   fon9::TimeInterval ti;
   CheckToStrRev(ti, &fon9::ToStrRev, "ToStrRev(TimeInterval)", "0");

   ti += fon9::TimeInterval_Microsecond(456);
   CheckToStrRev(ti, &fon9::ToStrRev, "                      ", "456 us");
   ti += fon9::TimeInterval_Millisecond(123);
   CheckToStrRev(ti, &fon9::ToStrRev, "                      ", "123.456 ms");
   ti += fon9::TimeInterval_Second(7);
   CheckToStrRev(ti, &fon9::ToStrRev, "                      ", "7.123456");
   ti += fon9::TimeInterval_Minute(8);
   CheckToStrRev(ti, &fon9::ToStrRev, "                      ", "08:07.123456");
   ti += fon9::TimeInterval_Hour(9);
   CheckToStrRev(ti, &fon9::ToStrRev, "                      ", "09:08:07.123456");
   ti += fon9::TimeInterval_Day(3);
   CheckToStrRev(ti, &fon9::ToStrRev, "                      ", "3-09:08:07.123456");

   typedef char* (*FnToStrRev)(char*, fon9::TimeInterval);
   FnToStrRev fnToStrRevFmt = [](char* pout, fon9::TimeInterval t) {
      return fon9::ToStrRev(pout, t, fon9::FmtDef{"+020."});
   };
   ti = fon9::TimeInterval{};
   ti += fon9::TimeInterval_Microsecond(456);
   CheckToStrRev(ti, fnToStrRevFmt, "(TimeInterval,FmtDef) ", "+0000000000-0.000456");
   ti += fon9::TimeInterval_Millisecond(123);
   CheckToStrRev(ti, fnToStrRevFmt, "                      ", "+0000000000-0.123456");
   ti += fon9::TimeInterval_Second(7);
   CheckToStrRev(ti, fnToStrRevFmt, "                      ", "+0000000000-7.123456");
   ti += fon9::TimeInterval_Minute(8);
   CheckToStrRev(ti, fnToStrRevFmt, "                      ", "+000000-08:07.123456");
   ti += fon9::TimeInterval_Hour(9);
   CheckToStrRev(ti, fnToStrRevFmt, "                      ", "+000-09:08:07.123456");
   ti += fon9::TimeInterval_Day(3);
   CheckToStrRev(ti, fnToStrRevFmt, "                      ", "+003-09:08:07.123456");

   fnToStrRevFmt = [](char* pout, fon9::TimeInterval t) {
      return fon9::ToStrRev(pout, t, fon9::FmtDef{"-+020."});
   };
   ti = fon9::TimeInterval{};
   ti += fon9::TimeInterval_Microsecond(456);
   CheckToStrRev(ti, fnToStrRevFmt, "(TimeInterval,FmtDef-)", "+0-0.000456         ");
   ti += fon9::TimeInterval_Millisecond(123);
   CheckToStrRev(ti, fnToStrRevFmt, "                      ", "+0-0.123456         ");
   ti += fon9::TimeInterval_Second(7);
   CheckToStrRev(ti, fnToStrRevFmt, "                      ", "+0-7.123456         ");
   ti += fon9::TimeInterval_Minute(8);
   CheckToStrRev(ti, fnToStrRevFmt, "                      ", "+0-08:07.123456     ");
   ti += fon9::TimeInterval_Hour(9);
   CheckToStrRev(ti, fnToStrRevFmt, "                      ", "+0-09:08:07.123456  ");
   ti += fon9::TimeInterval_Day(3);
   CheckToStrRev(ti, fnToStrRevFmt, "                      ", "+3-09:08:07.123456  ");
}
void TimeStamp_TestToStrRev() {
   fon9::TimeStamp ts{fon9::TimeInterval_Second(fon9::YYYYMMDDHHMMSS_ToEpochSeconds(20180301102030))};
   ts += fon9::TimeStamp::Make<6>(123456);
   if (ts.GetOrigValue() != 1519899630123456LL) {
      std::cout << "[ERROR] YYYYMMDDHHMMSS_ToEpochSeconds()";
   }
   CheckToStrRev(ts, &fon9::ToStrRev,       "ToStrRev(TimeStamp)", "20180301102030.123456");
   CheckToStrRev(ts, &fon9::ToStrRev_FIX,   "ToStrRev_FIX       ", "20180301-10:20:30");
   CheckToStrRev(ts, &fon9::ToStrRev_FIXMS, "ToStrRev_FIXMS     ", "20180301-10:20:30.123");
}
void TimeZoneOffset_TestToStrRev() {
   CheckToStrRev(fon9::TimeZoneOffset{},                        &fon9::ToStrRev, "ToStrRev(TimeZoneOffset)", "+0");
   CheckToStrRev(fon9::TimeZoneOffset::FromOffsetMinutes(30),   &fon9::ToStrRev, "                        ", "+0:30");
   CheckToStrRev(fon9::TimeZoneOffset::FromOffsetMinutes(-30),  &fon9::ToStrRev, "                        ", "-0:30");
   CheckToStrRev(fon9::TimeZoneOffset::FromOffsetHHMM(8, 0),    &fon9::ToStrRev, "                        ", "+8");
   CheckToStrRev(fon9::TimeZoneOffset::FromOffsetHHMM(8, 30),   &fon9::ToStrRev, "                        ", "+8:30");
   CheckToStrRev(fon9::TimeZoneOffset::FromOffsetHHMM(-8, -30), &fon9::ToStrRev, "                        ", "-8:30");

   typedef char* (*FnToStrRev)(char*, fon9::TimeZoneOffset);
   FnToStrRev fnToStrRevFmt = [](char* pout, fon9::TimeZoneOffset tzofs) {
      return fon9::ToStrRev(pout, tzofs, fon9::FmtDef{"-05."});
   };
   CheckToStrRev(fon9::TimeZoneOffset{},                        fnToStrRevFmt, "(TimeZoneOffset,FmtDef) ", "+0   ");
   CheckToStrRev(fon9::TimeZoneOffset::FromOffsetMinutes(30),   fnToStrRevFmt, "                        ", "+0:30");
   CheckToStrRev(fon9::TimeZoneOffset::FromOffsetMinutes(-30),  fnToStrRevFmt, "                        ", "-0:30");
   CheckToStrRev(fon9::TimeZoneOffset::FromOffsetHHMM(8, 0),    fnToStrRevFmt, "                        ", "+8   ");
   CheckToStrRev(fon9::TimeZoneOffset::FromOffsetHHMM(8, 30),   fnToStrRevFmt, "                        ", "+8:30");
   CheckToStrRev(fon9::TimeZoneOffset::FromOffsetHHMM(-8, -30), fnToStrRevFmt, "                        ", "-8:30");
}

//--------------------------------------------------------------------------//

template <class TimeT>
void CheckStrTo(const char* str, TimeT expectResult, int endpos = -1, TimeT nullValue = TimeT::Null()) {
   const char*     endptr;
   TimeT           ts = fon9::StrTo(fon9::StrView_cstr(str), nullValue, &endptr);
   fon9::NumOutBuf nbuf;
   nbuf.SetEOS();
   if (endpos < 0)
      endpos = static_cast<int>(strlen(str));
   int lenUsed = static_cast<int>(endptr - str);
   std::cout << "[     ] StrTo(\"" << str << "\")=" << fon9::ToStrRev(nbuf.end(), ts);
   if (ts == expectResult && endpos == lenUsed) {
      std::cout << "\r[OK   ]" << std::endl;
      return;
   }
   std::cout << "|expect=" << fon9::ToStrRev(nbuf.end(), expectResult);
   if (endpos != lenUsed)
      std::cout << "|endpos=" << lenUsed << "|expectEndPos=" << endpos;
   std::cout << "\r[ERROR] " << std::endl;
   abort();
}
void TimeInterval_TestStrTo() {
   // 1:23
   fon9::TimeInterval ti{fon9::TimeInterval_HHMMSS(0, 1, 23)};
   CheckStrTo("123         ", ti, 3);
   CheckStrTo("123       ? ", ti, 3);
   CheckStrTo("123  -      ", ti, 3);
   CheckStrTo("123 -     ? ", ti, 3);

   // 1:23.456
   ti += fon9::TimeInterval_Millisecond(456);
   CheckStrTo("123.456     ", ti, 7);
   CheckStrTo("123.456     ", ti, 7);
   CheckStrTo("123.456   ? ", ti, 7);
   CheckStrTo("123.456m  ? ", ti, 7);

   // 5-12:34:56.789012
   ti = fon9::TimeInterval_Day(5) + fon9::TimeInterval_HHMMSS(12, 34, 56) + fon9::TimeInterval_Microsecond(789012);
   //          0123456789-123456789-12
   CheckStrTo("5-12:34:56.789012     ", ti, 17);
   CheckStrTo("5-12 : 34 : 56 .789012", ti, 22);
   CheckStrTo("5-123456.789012       ", ti, 15);
   CheckStrTo("5-123456  .789012     ", ti, 17);
   // 5-34:56.789012
   ti = fon9::TimeInterval_Day(5) + fon9::TimeInterval_HHMMSS(0, 34, 56) + fon9::TimeInterval_Microsecond(789012);
   CheckStrTo("5-34:56.789012        ", ti, 14);
   CheckStrTo("5- 34 : 56 .789012    ", ti, 18);
   CheckStrTo("5-3456.789012         ", ti, 13);
   CheckStrTo("5-3456  .789012       ", ti, 15);
   // 5-56.789012
   ti = fon9::TimeInterval_Day(5) + fon9::TimeInterval_HHMMSS(0, 0, 56) + fon9::TimeInterval_Microsecond(789012);
   CheckStrTo("5-56.789012           ", ti, 11);
   CheckStrTo("5- 56 .789012         ", ti, 13);
   CheckStrTo("5-56.789012           ", ti, 11);
   CheckStrTo("5-56  .789012         ", ti, 13);
   // 5-56
   ti = fon9::TimeInterval_Day(5) + fon9::TimeInterval_HHMMSS(0, 0, 56);
   CheckStrTo("5-56                  ", ti, 4);
   CheckStrTo("5- 56                 ", ti, 5);
   CheckStrTo("5-0:56                ", ti, 6);
   CheckStrTo("5-0:0:56              ", ti, 8);
   CheckStrTo("5-0:0:56  xxxx        ", ti, 10);

   // ms, us, ns
   //          0123456789-12
   CheckStrTo("123.456ms   ", fon9::TimeInterval_Microsecond(123456), 9);
   CheckStrTo("123.45  ms  ", fon9::TimeInterval_Microsecond(123450), 10);
   CheckStrTo("123.456 us  ", fon9::TimeInterval_Microsecond(123), 10);
   CheckStrTo("123.567 us  ", fon9::TimeInterval_Microsecond(124), 10);
   CheckStrTo("123567.89 ns", fon9::TimeInterval_Microsecond(124), 12);

   // FormatError
   CheckStrTo("5- 56 :         FmtErr", fon9::TimeInterval::Null(), 7);
   CheckStrTo("5- 56 : 78 :    FmtErr", fon9::TimeInterval::Null(), 12);
}
void TimeStamp_TestStrTo() {
   fon9::TimeStamp ts{fon9::TimeInterval_Second(fon9::YYYYMMDDHHMMSS_ToEpochSeconds(20180301102030))};
   //          0123456789-123456789-123456
   CheckStrTo("20180301-10:20:30         ", ts, 17);
   CheckStrTo("20180301102030            ", ts, 14);
   CheckStrTo("20180301102030   .123     ", ts, 14);//小數點必須緊接在數字之後.
   ts += fon9::TimeStamp::Make<3>(123);
   CheckStrTo("20180301-10:20:30.123     ", ts, 21);
   CheckStrTo("20180301102030.123        ", ts, 18);
   ts += fon9::TimeStamp::Make<6>(456);
   CheckStrTo("20180301102030.123456     ", ts, 21);
   CheckStrTo("20180301102030.123456.123 ", ts, 21);
   CheckStrTo("20180301 - 102030.123456  ", ts, 24);

   CheckStrTo("20180301-10:20:30.123456  ", ts, 24);
   CheckStrTo("20180301 10:20:30.123456  ", ts, 24);
   CheckStrTo("2018/03/01 10:20:30.123456", ts, 26);
   CheckStrTo("2018/03/01-10:20:30.123456", ts, 26);
   CheckStrTo("2018-03-01 102030.123456  ", ts, 24);

   CheckStrTo("20180301 ?                ", fon9::TimeStamp{fon9::TimeInterval_Second(fon9::YYYYMMDDHHMMSS_ToEpochSeconds(20180301000000))}, 8);
   CheckStrTo("20180301102030 ?          ", fon9::TimeStamp{fon9::TimeInterval_Second(fon9::YYYYMMDDHHMMSS_ToEpochSeconds(20180301102030))}, 14);
}
void TimeZoneOffset_TestStrTo() {
   fon9::TimeZoneOffset tzNull{};
   //          012345678
   CheckStrTo("+L      ", fon9::GetLocalTimeZoneOffset(),                2, tzNull);
   CheckStrTo("-L      ", fon9::GetLocalTimeZoneOffset(),                2, tzNull);
   CheckStrTo("+TW     ", fon9::TimeZoneOffset::FromOffsetHHMM(8, 0),    3, tzNull);
   CheckStrTo("-TW     ", fon9::TimeZoneOffset::FromOffsetHHMM(8, 0),    3, tzNull);
   CheckStrTo("+'TW'   ", fon9::TimeZoneOffset::FromOffsetHHMM(8, 0),    5, tzNull);
   CheckStrTo("-'TW'   ", fon9::TimeZoneOffset::FromOffsetHHMM(8, 0),    5, tzNull);

   CheckStrTo("+8      ", fon9::TimeZoneOffset::FromOffsetHHMM(8, 0),    2, tzNull);
   CheckStrTo("+8:30   ", fon9::TimeZoneOffset::FromOffsetHHMM(8,30),    5, tzNull);
   CheckStrTo("-8      ", fon9::TimeZoneOffset::FromOffsetHHMM(-8, 0),   2, tzNull);
   CheckStrTo("-8:30   ", fon9::TimeZoneOffset::FromOffsetHHMM(-8,-30),  5, tzNull);

   CheckStrTo("+800    ", fon9::TimeZoneOffset::FromOffsetHHMM(8, 0),    4, tzNull);
   CheckStrTo("+830    ", fon9::TimeZoneOffset::FromOffsetHHMM(8, 30),   4, tzNull);
   CheckStrTo("-800    ", fon9::TimeZoneOffset::FromOffsetHHMM(-8, 0),   4, tzNull);
   CheckStrTo("-830    ", fon9::TimeZoneOffset::FromOffsetHHMM(-8, -30), 4, tzNull);
   
   CheckStrTo("+030    ", fon9::TimeZoneOffset::FromOffsetMinutes(30),   4, tzNull);
   CheckStrTo("-030    ", fon9::TimeZoneOffset::FromOffsetMinutes(-30),  4, tzNull);
   CheckStrTo("+030",     fon9::TimeZoneOffset::FromOffsetMinutes(30),   4, tzNull);
   CheckStrTo("-030",     fon9::TimeZoneOffset::FromOffsetMinutes(-30),  4, tzNull);
}

//--------------------------------------------------------------------------//

template <class TimeAux>
void TimeFunc_Benchmark(const char* fnName) {
   TimeAux           tprev, tnow;
   unsigned          countChanged = 0;
   fon9::StopWatch   stopWatch;

   stopWatch.ResetTimer();
   for (unsigned L = 0; L < kTimes; L++) {
      tnow.GetNow();
      if (tprev != tnow) {
         tprev = tnow;
         ++countChanged;
      }
   }
   stopWatch.PrintResultNoEOL(fnName, kTimes) << "|changed=" << std::setw(10) << countChanged << '\n';
}

void TimeStamp_GetNow_Benchmark() {
   // 測試各種取得時間戳的函式的效能.
   struct Aux_timeval {
      struct timeval tv{0,0};
      void GetNow() {
         gettimeofday(&this->tv, nullptr);
      }
      bool operator!=(const Aux_timeval& rhs) const {
         return (this->tv.tv_sec != rhs.tv.tv_sec || this->tv.tv_usec != rhs.tv.tv_usec);
      }
   };
   TimeFunc_Benchmark<Aux_timeval>("gettimeofday()          ");

   struct Aux_chrono_high_resolution_clock {
      std::chrono::high_resolution_clock::time_point  hrestp{};
      void GetNow() {
         this->hrestp = std::chrono::high_resolution_clock::now();
      }
      bool operator!=(const Aux_chrono_high_resolution_clock& rhs) const {
         return (this->hrestp != rhs.hrestp);
      }
   };
   TimeFunc_Benchmark<Aux_chrono_high_resolution_clock>("high_resolution_clock   ");

   struct Aux_chrono_system_clock {
      intmax_t us{0};
      void GetNow() {
         this->us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
      }
      bool operator!=(const Aux_chrono_system_clock& rhs) const {
         return (this->us != rhs.us);
      }
   };
   TimeFunc_Benchmark<Aux_chrono_system_clock>("chrono::system_time     ");

   struct Aux_fon9_TimeStamp {
      fon9::TimeStamp ts;
      void GetNow() {
         this->ts = fon9::UtcNow();
      }
      bool operator!=(const Aux_fon9_TimeStamp& rhs) const {
         return (this->ts != rhs.ts);
      }
   };
   TimeFunc_Benchmark<Aux_chrono_system_clock>("fon9::TimeStamp         ");
}

//--------------------------------------------------------------------------//

// FIX protocol time format: "yyyymmdd-HH:MM:SS"
void TimeStamp_StrConv_Benchmark() {
   fon9::StopWatch   stopWatch;

   time_t utctime;
   time(&utctime);
   fon9::NumOutBuf nbuf;
   nbuf.SetEOS();

   stopWatch.ResetTimer();
   for (unsigned L = 0; L < kTimes; L++) {
      fon9::EpochSecondsInfo(utctime + L);
   }
   double spanAdj = stopWatch.StopTimer();
   stopWatch.PrintResultNoEOL(spanAdj, "EpochSecondsInfo", kTimes)
      << "|'time_t' to 'struct tm'\n" << std::endl;

   stopWatch.ResetTimer();
   for (unsigned L = 0; L < kTimes; L++) {
      const struct tm& tmInfo = fon9::EpochSecondsInfo(utctime + L);
      strftime(nbuf.BufferEOS_, sizeof(nbuf.BufferEOS_), "%Y%m%d-%T", &tmInfo);
   }
   stopWatch.PrintResultNoEOL(stopWatch.StopTimer() - spanAdj, "strftime        ", kTimes)
      << "|last=" << nbuf.BufferEOS_ << std::endl;

   stopWatch.ResetTimer();
   for (unsigned L = 0; L < kTimes; L++) {
      ToStrRev_FIX(nbuf.end(), fon9::EpochSecondsToTimeStamp(utctime + L));
   }
   stopWatch.PrintResultNoEOL(stopWatch.StopTimer() - spanAdj, "ToStrRev_FIX    ", kTimes)
      << "|last=" << (nbuf.end() - fon9::kDateTimeStrWidth_FIX) << std::endl;

   stopWatch.ResetTimer();
   for (unsigned L = 0; L < kTimes; L++) {
      ToStrRev(nbuf.end(), fon9::EpochSecondsToTimeStamp(utctime + L), fon9::FmtTS{"Ymd-T"});
   }
   stopWatch.PrintResultNoEOL(stopWatch.StopTimer() - spanAdj, "ToStrRev(FmtTS) ", kTimes)
      << "|last=" << (nbuf.end() - fon9::kDateTimeStrWidth_FIX) << std::endl;
}

//--------------------------------------------------------------------------//

void Test_FmtTS() {
#define TEST_YYYYMMDDHHMMSS      20180311054530
#define TEST_Year                2018
#define TEST_Month               03
#define TEST_Day                 11
#define TEST_Hour                05
#define TEST_Minute              45
#define TEST_Second              30
#define TEST_us                  987654
#define TEST_cstr_MAKE_YMD(s)    fon9_CTXTOCSTR(TEST_Year) s fon9_CTXTOCSTR(TEST_Month) s fon9_CTXTOCSTR(TEST_Day)
#define TEST_cstr_MAKE_HMS(s)    fon9_CTXTOCSTR(TEST_Hour) s fon9_CTXTOCSTR(TEST_Minute) s fon9_CTXTOCSTR(TEST_Second)
#define TEST_cstr_us             "." fon9_CTXTOCSTR(TEST_us)
#define ADD_US(s)                s TEST_cstr_us
#define TEST_cstr_YMDHMS         fon9_CTXTOCSTR(TEST_YYYYMMDDHHMMSS)
#define TEST_cstr_YsMsD_H_M_S    TEST_cstr_MAKE_YMD("/") "-" TEST_cstr_MAKE_HMS(":")
#define TEST_cstr_Y_M_D_H_M_S    TEST_cstr_MAKE_YMD("-") "-" TEST_cstr_MAKE_HMS(":")
#define TEST_cstr_YsMsD_HMS      TEST_cstr_MAKE_YMD("/") "-" TEST_cstr_MAKE_HMS("")
#define TEST_cstr_YMD_H_M_S      TEST_cstr_MAKE_YMD("") "-" TEST_cstr_MAKE_HMS(":")
#define TEST_cstr_YMD_HMS        TEST_cstr_MAKE_YMD("") "-" TEST_cstr_MAKE_HMS("")

   struct Aux {
      fon9_NON_COPY_NON_MOVE(Aux);
      const fon9::TimeStamp TS_{fon9::YYYYMMDDHHMMSS_ToTimeStamp(TEST_YYYYMMDDHHMMSS) + fon9::TimeInterval_Microsecond(TEST_us)};
      Aux() = default;

      static void CheckTS(fon9::StrView str, const fon9::FmtTS& fmt, fon9::TimeStamp expected) {
         fon9::TimeStamp strts = StrTo(str, fon9::TimeStamp::Null());
         if (fon9::IsEnumContains(fmt.Flags_, fon9::FmtFlag::HasPrecision)) {
            if (strts == expected)
               return;
         }
         else {
            if (strts.GetIntPart() == expected.GetIntPart())
               return;
         }
         fon9::NumOutBuf nbuf;
         nbuf.SetEOS();
         std::cout //<< "StrTo.TimeStamp(", fon9::ToStrRev(nbuf.end(), ts) << ")"
            << "\r[ERROR]" << std::endl;
         abort();
      }
      void CheckFmtTS(const char* cstrfmt, fon9::TimeZoneOffset tzofs = fon9::TimeZoneOffset{}, const char* expected = nullptr) {
         fon9::StrView fmtstr = fon9::StrView_cstr(cstrfmt);
         std::cout << "[TEST ] fmt=\"" << cstrfmt << "\"" << std::setw(static_cast<int>(16 - fmtstr.size())) << "";

         fon9::FmtTS     fmt{fmtstr};
         fon9::NumOutBuf nbuf;
         nbuf.SetEOS();
         char*  pout = ToStrRev(nbuf.end(), this->TS_, fmt);
         std::cout << "|result=\"" << pout << "\"";
         if (expected && strcmp(expected, pout) != 0) {
            std::cout << "|expect=\"" << expected << "\"";
            abort();
         }
         this->CheckTS(fon9::StrView(pout, nbuf.end()), fmt, this->TS_ + tzofs);
         std::cout << std::endl;
      }
      void CheckFmtTS(const char* cstrfmt, const char* expected) {
         this->CheckFmtTS(cstrfmt, fon9::TimeZoneOffset{}, expected);
      }
   };
   Aux aux;
   aux.CheckFmtTS("",   TEST_cstr_YMDHMS);               //yyyymmddHHMMSS
   aux.CheckFmtTS("L",  TEST_cstr_YMDHMS);               //yyyymmddHHMMSS
   aux.CheckFmtTS(".",  ADD_US(TEST_cstr_YMDHMS));       //yyyymmddHHMMSS.uuuuuu
   aux.CheckFmtTS("L.", ADD_US(TEST_cstr_YMDHMS));       //yyyymmddHHMMSS.uuuuuu

   aux.CheckFmtTS("K-T",  TEST_cstr_YsMsD_H_M_S);        //YYYY/MM/DD-HH:MM:SS
   aux.CheckFmtTS("K-T.", ADD_US(TEST_cstr_YsMsD_H_M_S));//YYYY/MM/DD-HH:MM:SS.uuuuuu
   aux.CheckFmtTS("K-t",  TEST_cstr_YsMsD_HMS);          //YYYY/MM/DD-HHMMSS
   aux.CheckFmtTS("K-t.", ADD_US(TEST_cstr_YsMsD_HMS));  //YYYY/MM/DD-HHMMSS.uuuuuu

   aux.CheckFmtTS("f-T",  TEST_cstr_YMD_H_M_S);          //YYYYMMDD-HH:MM:SS
   aux.CheckFmtTS("f-T.", ADD_US(TEST_cstr_YMD_H_M_S));  //YYYYMMDD-HH:MM:SS.uuuuuu
   aux.CheckFmtTS("f-t",  TEST_cstr_YMD_HMS);            //YYYYMMDD-HHMMSS
   aux.CheckFmtTS("f-t.", ADD_US(TEST_cstr_YMD_HMS));    //YYYYMMDD-HHMMSS.uuuuuu

   aux.CheckFmtTS("F-T",    TEST_cstr_Y_M_D_H_M_S);      //YYYY-MM-DD-HH:MM:SS
   aux.CheckFmtTS("Y-m-d H:M:S",
                  fon9_CTXTOCSTR(TEST_Year)
                  "-" fon9_CTXTOCSTR(TEST_Month)
                  "-" fon9_CTXTOCSTR(TEST_Day)
                  " " fon9_CTXTOCSTR(TEST_Hour)
                  ":" fon9_CTXTOCSTR(TEST_Minute)
                  ":" fon9_CTXTOCSTR(TEST_Second));

   aux.CheckFmtTS("K-T+8",     fon9::TimeZoneOffset::FromOffsetHHMM(8, 0));
   aux.CheckFmtTS("K-T+'TW'",  fon9::TimeZoneOffset::FromOffsetHHMM(8, 0));
   aux.CheckFmtTS("K-T+'L'",   fon9::GetLocalTimeZoneOffset());
   aux.CheckFmtTS("K-T+800",   fon9::TimeZoneOffset::FromOffsetHHMM(8, 0));
   aux.CheckFmtTS("K-T+0800",  fon9::TimeZoneOffset::FromOffsetHHMM(8, 0));
   aux.CheckFmtTS("K-T+8:00",  fon9::TimeZoneOffset::FromOffsetHHMM(8, 0));
   aux.CheckFmtTS("K-T+08:00", fon9::TimeZoneOffset::FromOffsetHHMM(8, 0));
   aux.CheckFmtTS("K-T-8",     fon9::TimeZoneOffset::FromOffsetHHMM(-8, 0));
   aux.CheckFmtTS("K-T-800",   fon9::TimeZoneOffset::FromOffsetHHMM(-8, 0));
   aux.CheckFmtTS("K-T-0800",  fon9::TimeZoneOffset::FromOffsetHHMM(-8, 0));
   aux.CheckFmtTS("K-T-8:00",  fon9::TimeZoneOffset::FromOffsetHHMM(-8, 0));
   aux.CheckFmtTS("K-T-08:00", fon9::TimeZoneOffset::FromOffsetHHMM(-8, 0));
   aux.CheckFmtTS("K-T+830",   fon9::TimeZoneOffset::FromOffsetHHMM(8, 30));
   aux.CheckFmtTS("K-T+0830",  fon9::TimeZoneOffset::FromOffsetHHMM(8, 30));
   aux.CheckFmtTS("K-T+8:30",  fon9::TimeZoneOffset::FromOffsetHHMM(8, 30));
   aux.CheckFmtTS("K-T+08:30", fon9::TimeZoneOffset::FromOffsetHHMM(8, 30));
   aux.CheckFmtTS("K-T-830",   fon9::TimeZoneOffset::FromOffsetHHMM(-8, -30));
   aux.CheckFmtTS("K-T-0830",  fon9::TimeZoneOffset::FromOffsetHHMM(-8, -30));
   aux.CheckFmtTS("K-T-8:30",  fon9::TimeZoneOffset::FromOffsetHHMM(-8, -30));
   aux.CheckFmtTS("K-T-08:30", fon9::TimeZoneOffset::FromOffsetHHMM(-8, -30));
}

//--------------------------------------------------------------------------//

int main() {
   fon9::AutoPrintTestInfo utinfo{"TimeStamp"};

   TimeInterval_TestToStrRev();
   TimeInterval_TestStrTo();

   utinfo.PrintSplitter();
   TimeZoneOffset_TestToStrRev();
   TimeZoneOffset_TestStrTo();

   utinfo.PrintSplitter();
   time_t utctime;
   time(&utctime);
   struct tm* loTimeInfo = localtime(&utctime);
   fon9::NumOutBuf nbuf;
   nbuf.SetEOS();
   strftime(nbuf.BufferEOS_, sizeof(nbuf.BufferEOS_), "%F %T", loTimeInfo);
   std::cout << "[     ] localtime=" << nbuf.BufferEOS_;
   fon9::TimeStamp ts = fon9::EpochSecondsToTimeStamp(utctime) + fon9::GetLocalTimeZoneOffset();
   if (fon9::stdtm_ToEpochSeconds(*loTimeInfo)  != ts.ToEpochSeconds()) {
      std::cout << "|fon9.LocalTime=" << fon9::ToStrRev(nbuf.end(), ts);
      std::cout << "\r[ERROR]" << std::endl;
      abort();
   }
   std::cout << "\r[OK   ]" << std::endl;

   utinfo.PrintSplitter();
   TimeStamp_TestToStrRev();
   TimeStamp_TestStrTo();

   utinfo.PrintSplitter();
   Test_FmtTS();

   utinfo.PrintSplitter();
   TimeStamp_GetNow_Benchmark();

   utinfo.PrintSplitter();
   TimeStamp_StrConv_Benchmark();
}
