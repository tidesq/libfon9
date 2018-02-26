// \file fon9/StrTo_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "fon9/Decimal.hpp"
#include "fon9/TestTools.hpp"
#include "fon9/TypeName.hpp"
#include <inttypes.h>//strtoimax()
#include <vector>
#include <cctype>

template <class I, fon9::DecScaleT S>
std::ostream& operator<<(std::ostream& os, const fon9::Decimal<I,S>& v) {
   return(os << fon9::To<double>(v));
}

using AuxToInt64 = fon9::StrToIntAux<int64_t>;
using AuxToInt64U = fon9::StrToIntAux<uint64_t>;
using AuxToInt32 = fon9::StrToIntAux<int32_t>;

using Fd64d5 = fon9::Decimal<int64_t, 5>;
using AuxToFd64d5 = fon9::StrToDecimalAux<Fd64d5>;

using Fd64d5U = fon9::Decimal<uint64_t, 5>;
using AuxToFd64d5U = fon9::StrToDecimalAux<Fd64d5U>;

using Fd32d5 = fon9::Decimal<int32_t, 5>;
using AuxToFd32d5 = fon9::StrToDecimalAux<Fd32d5>;

template <class AuxT>
void StrTo_Test_Item(const char* str, typename AuxT::ResultT expectResult, unsigned expectUsedStrLen) {
   using ResultT = typename AuxT::ResultT;
   std::string msgstr = std::string("\"") + str + "\"";
   std::cout << "[=====] Str=" << std::setw(24) << std::left << msgstr
      << "|To=" << std::setw(27) << fon9::TypeName::Make<ResultT>().get()
      << "|Result=";

   AuxT        aux{};
   ResultT     res{};
   const char* pend = StrToNum(fon9::StrView_cstr(str), aux, res);
   std::cout << res;

   if (res == expectResult) {
      const char* expectPos = str + expectUsedStrLen;
      if (pend == expectPos) {
         std::cout << "\r[OK   ] " << std::endl;
         return;
      }
      std::cout << "\n[ERROR] ExpectPos=\"" << expectPos << "\"|ErrorPos=\"" << pend << "\"\n        " << std::endl;;
   }
   else
      std::cout << "\n[ERROR] Expect=" << expectResult << "|Result=" << res << "\n        " << std::endl;;
   abort();
}

void StrTo_Test() {
   #define ASSERT_TEST(expression)              \
      if(!(expression)) {                       \
         std::cout << "[ERROR] AssertFail=\""   \
                   << #expression               \
                   << "\""                      \
                   << std::endl;                \
         abort();                               \
      }
   ASSERT_TEST(fon9::StrTo("", 999) == 999);
   ASSERT_TEST(fon9::StrTo("123", 999) == 123);
   ASSERT_TEST(fon9::StrToDec("", 3, 999999) == 999999)
   ASSERT_TEST(fon9::StrToDec("123.456", 3, 999999) == 123456);

   StrTo_Test_Item<AuxToInt64>("123456",          123456, 6);
   StrTo_Test_Item<AuxToInt64>("123456xyz",       123456, 6);
   StrTo_Test_Item<AuxToInt64>("123456 xyz",      123456, 6);
   StrTo_Test_Item<AuxToInt64>("+123456 xyz",     123456, 7);
   StrTo_Test_Item<AuxToInt64>("-123456 xyz",    -123456, 7);
   StrTo_Test_Item<AuxToInt64>("   123456 xyz",   123456, 9);
   StrTo_Test_Item<AuxToInt64>("+  123456 xyz",   123456, 9);
   StrTo_Test_Item<AuxToInt64>("-  123456 xyz",  -123456, 9);
   StrTo_Test_Item<AuxToInt64>("  +123456 xyz",   123456, 9);
   StrTo_Test_Item<AuxToInt64>("  -123456 xyz",  -123456, 9);
   StrTo_Test_Item<AuxToInt64>("  +123456.xyz",   123456, 9);
   StrTo_Test_Item<AuxToInt64>("  -123456,xyz",  -123456, 9);
   StrTo_Test_Item<AuxToInt64>("  +123,456.xyz",  123,    6);
   StrTo_Test_Item<AuxToInt64>("  -123,456,xyz", -123,    6);
   StrTo_Test_Item<fon9::StrToSkipIntSepAux<AuxToInt64>>("  +123,456.xyz",  123456, 10);
   StrTo_Test_Item<fon9::StrToSkipIntSepAux<AuxToInt64>>("  -123,456,xyz", -123456, 11);

   StrTo_Test_Item<AuxToInt64U>("123456",        123456, 6);
   StrTo_Test_Item<AuxToInt64U>("123456xyz",     123456, 6);
   StrTo_Test_Item<AuxToInt64U>("123456 xyz",    123456, 6);
   StrTo_Test_Item<AuxToInt64U>("+123456 xyz",   123456, 7);
   StrTo_Test_Item<AuxToInt64U>("-123456 xyz",   0,      0);
   StrTo_Test_Item<AuxToInt64U>("   123456 xyz", 123456, 9);
   StrTo_Test_Item<AuxToInt64U>("+  123456 xyz", 123456, 9);
   StrTo_Test_Item<AuxToInt64U>("-  123456 xyz", 0,      0);
   StrTo_Test_Item<AuxToInt64U>("  +123456 xyz", 123456, 9);
   StrTo_Test_Item<AuxToInt64U>("  -123456 xyz", 0,      2);
   StrTo_Test_Item<fon9::StrToSkipIntSepAux<AuxToInt64U>>("  +123,456.xyz", 123456, 10);
   StrTo_Test_Item<fon9::StrToSkipIntSepAux<AuxToInt64U>>("  -123,456,xyz", 0,      2);

   StrTo_Test_Item<AuxToFd64d5>("123456.789",         Fd64d5::Make<3>(123456789),  10);
   StrTo_Test_Item<AuxToFd64d5>("123456.789xyz",      Fd64d5::Make<3>(123456789),  10);
   StrTo_Test_Item<AuxToFd64d5>("123456.789 xyz",     Fd64d5::Make<3>(123456789),  10);
   StrTo_Test_Item<AuxToFd64d5>("+123456.789 xyz",    Fd64d5::Make<3>(123456789),  11);
   StrTo_Test_Item<AuxToFd64d5>("-123456.789 xyz",    Fd64d5::Make<3>(-123456789), 11);
   StrTo_Test_Item<AuxToFd64d5>("   123456.789 xyz",  Fd64d5::Make<3>(123456789),  13);
   StrTo_Test_Item<AuxToFd64d5>("+  123456.789 xyz",  Fd64d5::Make<3>(123456789),  13);
   StrTo_Test_Item<AuxToFd64d5>("-  123456.789 xyz",  Fd64d5::Make<3>(-123456789), 13);
   StrTo_Test_Item<AuxToFd64d5>("  +123456.789 xyz",  Fd64d5::Make<3>(123456789),  13);
   StrTo_Test_Item<AuxToFd64d5>("  -123456.789 xyz",  Fd64d5::Make<3>(-123456789), 13);
   StrTo_Test_Item<AuxToFd64d5>("  +123456.789.xyz",  Fd64d5::Make<3>(123456789),  13);
   StrTo_Test_Item<AuxToFd64d5>("  -123456.789,xyz",  Fd64d5::Make<3>(-123456789), 13);
   StrTo_Test_Item<AuxToFd64d5>("  +123,456.789.xyz", Fd64d5::Make<0>(123),  6);
   StrTo_Test_Item<AuxToFd64d5>("  -123,456.789,xyz", Fd64d5::Make<0>(-123), 6);
   StrTo_Test_Item<fon9::StrToSkipIntSepAux<AuxToFd64d5>>("  +123,456.789.xyz", Fd64d5::Make<3>(123456789),  14);
   StrTo_Test_Item<fon9::StrToSkipIntSepAux<AuxToFd64d5>>("  -123,456.789,xyz", Fd64d5::Make<3>(-123456789), 14);

   StrTo_Test_Item<fon9::StrToSkipIntSepAux<AuxToFd64d5>>("+123,456.78978599.xyz", Fd64d5::Make<5>(12345678979),  17);
   StrTo_Test_Item<fon9::StrToSkipIntSepAux<AuxToFd64d5>>("-123,456.78978599,xyz", Fd64d5::Make<5>(-12345678979), 17);
   StrTo_Test_Item<fon9::StrToSkipIntSepAux<AuxToFd64d5>>("+123,456.78912399.xyz", Fd64d5::Make<5>(12345678912),  17);
   StrTo_Test_Item<fon9::StrToSkipIntSepAux<AuxToFd64d5>>("-123,456.78912399,xyz", Fd64d5::Make<5>(-12345678912), 17);

   StrTo_Test_Item<AuxToFd64d5U>("123456.789",         Fd64d5U::Make<3>(123456789), 10);
   StrTo_Test_Item<AuxToFd64d5U>("123456.789xyz",      Fd64d5U::Make<3>(123456789), 10);
   StrTo_Test_Item<AuxToFd64d5U>("123456.789 xyz",     Fd64d5U::Make<3>(123456789), 10);
   StrTo_Test_Item<AuxToFd64d5U>("+123456.789 xyz",    Fd64d5U::Make<3>(123456789), 11);
   StrTo_Test_Item<AuxToFd64d5U>("-123456.789 xyz",    Fd64d5U::Make<0>(0),         0);
   StrTo_Test_Item<AuxToFd64d5U>("   123456.789 xyz",  Fd64d5U::Make<3>(123456789), 13);
   StrTo_Test_Item<AuxToFd64d5U>("+  123456.789 xyz",  Fd64d5U::Make<3>(123456789), 13);
   StrTo_Test_Item<AuxToFd64d5U>("-  123456.789 xyz",  Fd64d5U::Make<0>(0),         0);
   StrTo_Test_Item<AuxToFd64d5U>("  +123456.789 xyz",  Fd64d5U::Make<3>(123456789), 13);
   StrTo_Test_Item<AuxToFd64d5U>("  -123456.789 xyz",  Fd64d5U::Make<0>(0),         2);
   StrTo_Test_Item<AuxToFd64d5U>("  +123456.789.xyz",  Fd64d5U::Make<3>(123456789), 13);
   StrTo_Test_Item<AuxToFd64d5U>("  -123456.789,xyz",  Fd64d5U::Make<0>(0),         2);
   StrTo_Test_Item<AuxToFd64d5U>("  +123,456.789.xyz", Fd64d5U::Make<0>(123),       6);
   StrTo_Test_Item<AuxToFd64d5U>("  -123,456.789,xyz", Fd64d5U::Make<0>(0),         2);
   StrTo_Test_Item<fon9::StrToSkipIntSepAux<AuxToFd64d5U>>("  +123,456.789.xyz", Fd64d5U::Make<3>(123456789), 14);
   StrTo_Test_Item<fon9::StrToSkipIntSepAux<AuxToFd64d5U>>("  -123,456.789,xyz", Fd64d5U::Make<0>(0),         2);

   StrTo_Test_Item<AuxToFd32d5>("+21474.83647", Fd32d5::Make<5>(2147483647),  12);// 2147483647 = 0x7fff ffff.
   StrTo_Test_Item<AuxToFd32d5>("+21474.83648", Fd32d5::max(),                12);
   StrTo_Test_Item<AuxToFd32d5>("-21474.83647", Fd32d5::Make<5>(-2147483647), 12);
   StrTo_Test_Item<AuxToFd32d5>("-21474.83648", Fd32d5::Make<5>(0x80000000),  12);//-2147483648 = 0x8000 0000.
   StrTo_Test_Item<AuxToFd32d5>("-21474.83649", Fd32d5::min(),                12);//-2147483648 = 0x8000 0000.

   // underflow => int32::min()
   StrTo_Test_Item<AuxToFd32d5>("-2147483.648:Underflow", Fd32d5::min(), 12);
   // -(1234567.89100 => 1C BE99 1A6C) =>(int32)=> underflow => int32.min()
   StrTo_Test_Item<AuxToFd32d5>("-1234567.891:Underflow", Fd32d5::min(), 12);

   //測試計算過程是否會 overflow, 是否有四捨五入.
   StrTo_Test_Item<AuxToFd32d5>("+10000.000001234", Fd32d5::Make<5>(1000000000),  16);
   StrTo_Test_Item<AuxToFd32d5>("-10000.000001234", Fd32d5::Make<5>(-1000000000), 16);
   StrTo_Test_Item<AuxToFd32d5>("+10000.000005234", Fd32d5::Make<5>(1000000001),  16);
   StrTo_Test_Item<AuxToFd32d5>("-10000.000005234", Fd32d5::Make<5>(-1000000001), 16);
   StrTo_Test_Item<AuxToFd32d5>("+21474.836465432", Fd32d5::Make<5>(2147483647),  16);
   StrTo_Test_Item<AuxToFd32d5>("-21474.836475432", Fd32d5::Make<5>(0x80000000),  16);//-2147483648
   StrTo_Test_Item<AuxToFd32d5>("+21474.836471234", Fd32d5::Make<5>(2147483647),  16);
   StrTo_Test_Item<AuxToFd32d5>("-21474.836471234", Fd32d5::Make<5>(-2147483647), 16);
   StrTo_Test_Item<AuxToFd32d5>("+21474.836465432", Fd32d5::Make<5>(2147483647),  16);
   StrTo_Test_Item<AuxToFd32d5>("-21474.836475432", Fd32d5::Make<5>(0x80000000),  16);//-2147483648
   //overflow!!
   StrTo_Test_Item<AuxToFd32d5>("+21474.836475432", Fd32d5::Make<5>(2147483647), 16);
   StrTo_Test_Item<AuxToFd32d5>("-21474.836485432", Fd32d5::Make<5>(0x80000000), 16);//-2147483648
   StrTo_Test_Item<AuxToFd32d5>("+21474836475432",  Fd32d5::Make<5>(2147483647), 15);
   StrTo_Test_Item<AuxToFd32d5>("-21474836475432",  Fd32d5::Make<5>(0x80000000), 15);
   StrTo_Test_Item<AuxToFd32d5>("+21474.999999999", Fd32d5::Make<5>(2147483647), 16);
   StrTo_Test_Item<AuxToFd32d5>("-21474.999999999", Fd32d5::Make<5>(0x80000000), 16);//-2147483648

   // 測試 overflow 之後的結果.
   StrTo_Test_Item<AuxToInt32>("+12345678900", 0x7FFFFFFF, 12);
   StrTo_Test_Item<AuxToInt32>("-12345678900", 0x80000000, 12);
   StrTo_Test_Item<AuxToInt32>("+2147483648",  0x7FFFFFFF, 11);
   StrTo_Test_Item<AuxToInt32>("-2147483648",  0x80000000, 11);
}

void StrTo_Benchmark_isfunc() {
   const unsigned    kTimes = 1000 * 1000;
   fon9::StopWatch   stopWatch;

   unsigned L;
   int64_t  chksum, sum;
   //------------------------------
#define TEST_isfunc(isfunc) \
      for (L = 0; L < 0x100; ++L) { \
         if (!fon9::isfunc(static_cast<unsigned char>(L)) != !isfunc(static_cast<unsigned char>(L))) { \
            std::cout << "[ERROR] fon9::" #isfunc "(#" << L << "='" << static_cast<char>(L) << "') = " \
                      << fon9::isfunc(static_cast<unsigned char>(L)) << std::endl; \
            abort(); \
         } \
      } \
      \
      sum = 0;\
      stopWatch.ResetTimer();  \
      for (L = 0; L < kTimes; ++L) \
         sum += (::isfunc(static_cast<unsigned char>(L)) != 0); \
      stopWatch.PrintResult("::" #isfunc "()      ", kTimes); \
      \
      chksum = 0; \
      stopWatch.ResetTimer(); \
      for (L = 0; L < kTimes; ++L) \
         chksum += (std::isfunc(static_cast<unsigned char>(L)) != 0); \
      stopWatch.PrintResult("std::" #isfunc "()   ", kTimes); \
      ASSERT_TEST(chksum == sum); \
      \
      chksum = 0; \
      stopWatch.ResetTimer(); \
      for (L = 0; L < kTimes; ++L) \
         chksum += (fon9::isfunc(static_cast<unsigned char>(L)) != 0); \
      stopWatch.PrintResult("fon9::" #isfunc "()  ", kTimes); \
      ASSERT_TEST(chksum == sum); \
      std::cout << "-----" << std::endl;
   //------------------------------
   TEST_isfunc(iscntrl);
   TEST_isfunc(isprint);
   TEST_isfunc(isspace);
   TEST_isfunc(isblank);
   TEST_isfunc(isgraph);
   TEST_isfunc(isalnum);
   TEST_isfunc(isupper);
   TEST_isfunc(islower);
   TEST_isfunc(isalpha);
   TEST_isfunc(isdigit);
}

void StrTo_Benchmark() {
   const unsigned    kTimes = 1000 * 1000;
   fon9::StopWatch   stopWatch;

   char     strbuf[128];
   unsigned L;
   int64_t  chksum, sum;
   using NumStrs = std::vector<std::string>;
   NumStrs  nums;
   sum = 0;
   nums.reserve(kTimes);
   for (int i = 0 - (kTimes / 2); i < static_cast<int>(kTimes / 2); ++i) {
      nums.emplace_back(strbuf, sprintf(strbuf, "%d", i));
      sum += i;
   }

   std::cout << "--- int64/imax ---" << std::endl;
   chksum = 0;
   stopWatch.ResetTimer();
   for (L = 0; L < kTimes; ++L)
      chksum += fon9::NaiveStrToSInt(&nums[L]);
   stopWatch.PrintResult("NaiveStrToSInt() ", kTimes);
   ASSERT_TEST(chksum == sum);

   chksum = 0;
   stopWatch.ResetTimer();
   for (L = 0; L < kTimes; ++L)
      chksum += fon9::StrTo(&nums[L], int64_t{});
   stopWatch.PrintResult("StrToNum(int64)  ", kTimes);
   ASSERT_TEST(chksum == sum);

   chksum = 0;
   stopWatch.ResetTimer();
   for (L = 0; L < kTimes; ++L)
      chksum += strtoimax(nums[L].c_str(), NULL, 10);
   stopWatch.PrintResult("strtoimax()      ", kTimes);
   ASSERT_TEST(chksum == sum);

   std::cout << "--- int32/long ---" << std::endl;
   chksum = 0;
   stopWatch.ResetTimer();
   for (L = 0; L < kTimes; ++L)
      chksum += fon9::StrTo(&nums[L], int32_t{});
   stopWatch.PrintResult("StrToNum(int32)  ", kTimes);
   ASSERT_TEST(chksum == sum);

   chksum = 0;
   stopWatch.ResetTimer();
   for (L = 0; L < kTimes; ++L)
      chksum += strtol(nums[L].c_str(), NULL, 10);
   stopWatch.PrintResult("strtol()         ", kTimes);
   ASSERT_TEST(chksum == sum);

   std::cout << "--- decimal/double ---" << std::endl;
   // double.
   using TestDecT = fon9::Decimal<int64_t, 5>;
   static const char cstrIntDec[] = "123456.7890000";
   TestDecT          decValue{fon9::StrTo(cstrIntDec, TestDecT{})};
   if (decValue.GetOrigValue() != 12345678900) {
      std::cout << "[ERROR] Str=" << cstrIntDec << "|Decimal.OrigValue=" << decValue.GetOrigValue() << std::endl;
      abort();
   }

   nums.clear();
   nums.reserve(kTimes);
   L = 0;
   double dsum = 0;
   for (int i = 0 - (kTimes / 2); i < static_cast<int>(kTimes / 2); ++i) {
      L = static_cast<unsigned>((L + 1) % TestDecT::Divisor);
      nums.emplace_back(strbuf, sprintf(strbuf, "%d.%0*u", i, TestDecT::Scale, L));
      sum = i * TestDecT::Divisor;
      if (i < 0)
         sum -= L;
      else
         sum += L;
      dsum += static_cast<double>(sum) / TestDecT::Divisor;
   }

   sprintf(strbuf, "StrToNum(i%u" "d" "%u)", static_cast<unsigned>(sizeof(TestDecT)) * 8u, TestDecT::Scale);
   std::string strDecT{strbuf};
   strDecT.append(17 - strDecT.size(), ' ');
   double dchksum = 0;
   stopWatch.ResetTimer();
   for (L = 0; L < kTimes; ++L)
      dchksum += fon9::StrTo(&nums[L], TestDecT{}).To<double>();
   stopWatch.PrintResult(strDecT.c_str(), kTimes);
   ASSERT_TEST(dchksum == dsum);

   dchksum = 0;
   stopWatch.ResetTimer();
   for (L = 0; L < kTimes; ++L)
      dchksum += strtod(nums[L].c_str(), nullptr);
   stopWatch.PrintResult("strtod()         ", kTimes);
   ASSERT_TEST(dchksum == dsum);
}

int main()
{
   std::cout <<
      "#####################################################\n"
      "fon9 StrTo test\n"
      "====================================================="
      << std::endl;
   std::cout << std::fixed << std::setprecision(9);
   std::cout.imbue(std::locale(""));

   StrTo_Test();
   std::cout << "-----------------------------------------------------" << std::endl;
   StrTo_Benchmark_isfunc();
#ifndef _DEBUG
   std::cout << "-----------------------------------------------------" << std::endl;
   StrTo_Benchmark();
#endif

   std::cout <<
      "=====================================================\n"
      "fon9 StrTo test # END #\n"
      "#####################################################\n"
      << std::endl;
}
