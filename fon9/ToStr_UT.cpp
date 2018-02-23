// \file fon9/ToStr_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "fon9/ToStr.hpp"
#include "fon9/StrTo.hpp"
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

void ToStr_benchmark() {
   const long        kTimes = 1000 * 1000;
   fon9::StopWatch   stopWatch;

   fon9::NumOutBuf   nbuf;
   nbuf.SetEOS();

   stopWatch.ResetTimer();
   for (long L = -kTimes / 2; L < kTimes / 2; L++)
      fon9::IntToStrRev(nbuf.end(), L);
   stopWatch.PrintResult("IntToStrRev()    ", kTimes);

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
}

int main() {
   std::cout <<
      "#####################################################\n"
      "fon9 ToStr test\n"
      "====================================================="
      << std::endl;
   std::cout << std::fixed << std::setprecision(9);
   std::cout.imbue(std::locale(""));

   fon9::NumOutBuf nbuf;
   nbuf.SetEOS();
   for (long L = -1234567; L < 1234567; ++L) {
      CheckToStrRevResult(fon9::IntToStrRev(nbuf.end(), L), 10, "IntToStrRev", L);
      CheckToStrRevResult(fon9::HexToStrRev(nbuf.end(), L), 16, "HexToStrRev", L);
      CheckToStrRevResult(fon9::BinToStrRev(nbuf.end(), L), 2, "BinToStrRev", L);
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

   std::cout << "-----------------------------------------------------" << std::endl;
   ToStr_benchmark();

   std::cout <<
      "=====================================================\n"
      "fon9 ToStr test # END #\n"
      "#####################################################\n"
      << std::endl;
}
