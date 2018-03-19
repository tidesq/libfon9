// \file fon9/ToStr_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "fon9/RevFormat.hpp"
#include "fon9/Decimal.hpp"
#include "fon9/TestTools.hpp"

void CheckToStrRevResult(const char* pstr, int base, const char* fnName, long expectResult) {
   long res = static_cast<long>(strtoul(pstr + (*pstr == '-'), nullptr, base));
   if (*pstr == '-')
      res = -res;
   if (res == expectResult)
      return;
   std::cout << "[ERROR] " << fnName << "(" << expectResult << ") output is '" << pstr << "'"
      ", but strtol() result is " << res << std::endl;
   abort();
}

template <size_t Width>
void TestPic9ToStrRev() {
   fon9::NumOutBuf   nbuf;
   nbuf.SetEOS();
   const char fmt[] = {'%', '0', static_cast<char>(Width + '0'), 'u', 0};
   for (unsigned L = 0; L < 1234567; ++L) {
      char* pout = fon9::Pic9ToStrRev<Width>(nbuf.end(), L);
      fon9_MSC_WARN_DISABLE(4774);//'sprintf' : format string expected in argument 2 is not a string literal
      sprintf(nbuf.Buffer_, fmt, L);
      fon9_MSC_WARN_POP;
      if (strcmp(nbuf.Buffer_, pout) != 0) {
         std::cout << "[ERROR] Pic9ToStrRev<" << Width << ">(" << L << ") output is '" << pout << "'" <<
            ", but expect result is " << nbuf.Buffer_ << std::endl;
         abort();
      }
   }
   std::cout << "[OK   ] " << "Pic9ToStrRev<" << Width << ">();\n";
}

void ToStr_benchmark() {
   const long        kTimes = 1000 * 1000;
   fon9::StopWatch   stopWatch;

   fon9::NumOutBuf   nbuf;
   nbuf.SetEOS();

   stopWatch.ResetTimer();
   for (long L = -kTimes / 2; L < kTimes / 2; L++)
      fon9::ToStrRev(nbuf.end(), L);
   stopWatch.PrintResult("ToStrRev(long)   ", kTimes);

#ifdef _MSC_VER
   stopWatch.ResetTimer();
   for (long L = -kTimes / 2; L < kTimes / 2; L++)
      _ltoa(L, nbuf.Buffer_, 10);
   stopWatch.PrintResult("_ltoa(long)      ", kTimes);
#endif

   stopWatch.ResetTimer();
   for (long L = -kTimes / 2; L < kTimes / 2; L++)
      std::to_string(L);
   stopWatch.PrintResult("to_string(long)  ", kTimes);

   stopWatch.ResetTimer();
   for (long L = -kTimes / 2; L < kTimes / 2; L++)
      sprintf(nbuf.Buffer_, "%ld", L);
   stopWatch.PrintResult("sprintf(long)    ", kTimes);

   fon9::RevBufferFixedSize<1024> rbuf;
   stopWatch.ResetTimer();
   for (long L = -kTimes / 2; L < kTimes / 2; L++) {
      rbuf.RewindEOS();
      fon9::RevPrint(rbuf, L);
   }
   stopWatch.PrintResult("RevPrint(rbuf)   ", kTimes);

   stopWatch.ResetTimer();
   for (long L = -kTimes / 2; L < kTimes / 2; L++) {
      rbuf.RewindEOS();
      fon9::RevFormat(rbuf, "{0}", L);
   }
   stopWatch.PrintResult("RevFormat(rbuf)  ", kTimes);

   stopWatch.ResetTimer();
   for (long L = -kTimes / 2; L < kTimes / 2; L++)
      fon9::ToStrRev(nbuf.end(), L, fon9::FmtDef{""});
   stopWatch.PrintResult("ToStrRev(Fmt)    ", kTimes);

   std::cout << "----- Pic9ToStrRev()/%09u -----\n";
   stopWatch.ResetTimer();
   for (unsigned L = 0; L < kTimes; L++)
      fon9::Pic9ToStrRev<9>(nbuf.end(), L);
   stopWatch.PrintResult("Pic9ToStrRev()   ", kTimes);

   stopWatch.ResetTimer();
   for (unsigned L = 0; L < kTimes; L++)
      sprintf(nbuf.Buffer_, "%09u", L);
   stopWatch.PrintResult("sprintf(%09u)    ", kTimes);
}

int main() {
   fon9::AutoPrintTestInfo utinfo{"ToStr"};

   fon9::NumOutBuf nbuf;
   nbuf.SetEOS();
   for (long L = -1234567; L < 1234567; ++L) {
      CheckToStrRevResult(fon9::ToStrRev(nbuf.end(), L),    10, "ToStrRev",    L);
      CheckToStrRevResult(fon9::HexToStrRev(nbuf.end(), L), 16, "HexToStrRev", L);
      CheckToStrRevResult(fon9::HEXToStrRev(nbuf.end(), L), 16, "HEXToStrRev", L);
      CheckToStrRevResult(fon9::OctToStrRev(nbuf.end(), L),  8, "OctToStrRev", L);
      CheckToStrRevResult(fon9::BinToStrRev(nbuf.end(), L),  2, "BinToStrRev", L);
   }
   std::cout << "[OK   ] " << "IntToStrRev(); HexToStrRev(); BinToStrRev();\n";

   const fon9::DecScaleT kDecScale = 3;
   for (long L = -1234567; L < 1234567; ++L) {
      char* pout = fon9::DecToStrRev(nbuf.end(), L, kDecScale);
      long  res = fon9::StrToDec(fon9::StrView{pout, nbuf.end()}, kDecScale, std::numeric_limits<long>::min());
      if (L != res) {
         std::cout << "[ERROR] DecToStrRev(" << L << ") output is '" << pout << "'"
            ", but StrToDec() result is " << res << std::endl;
         abort();
      }
   }
   std::cout << "[OK   ] " << "DecToStrRev();\n";

   using NumT = fon9::Decimal<long, kDecScale>;
   NumT  num;
   for (long L = -1234567; L < 1234567; ++L) {
      num.Assign<kDecScale>(L);
      char* pout = fon9::ToStrRev(nbuf.end(), num);
      NumT  res = fon9::StrTo(fon9::StrView{pout, nbuf.end()}, NumT::Null());
      if (num != res) {
         std::cout << "[ERROR] ToStrRev(" << num.To<double>() << ") output is '" << pout << "'"
            ", but StrTo() result is " << res.To<double>() << std::endl;
         abort();
      }
   }
   std::cout << "[OK   ] " << "Decimal: ToStrRev(); StrTo();\n";

   TestPic9ToStrRev<7>();
   TestPic9ToStrRev<8>();

   utinfo.PrintSplitter();

   ToStr_benchmark();
}
