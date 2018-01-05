/// \file fon9/TestTools.hpp
///
/// fon9 的簡易測試工具.
///
/// \author fonwinz@gmail.com
#ifndef __fon9_TestTools_hpp__
#define __fon9_TestTools_hpp__
#include "fon9/Utility.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>

namespace fon9 {

#ifdef _MSC_VER
   #define fon9_NOINLINE(s)   __declspec(noinline) s
#elif __GNUC__
   #define fon9_NOINLINE(s)   s __attribute__ ((noinline))
#else
   #define fon9_NOINLINE(s)   s
#endif

class AutoTimeUnit {
   double      Span_;
   const char* UnitStr_;
public:
   AutoTimeUnit(double secs) {
      this->Make(secs);
   }

   void Make(double secs) {
      static const char* cstrUnits[] = {
         "sec", "ms", "us", "ns"
      };
      bool isNeg = (secs < 0);
      if (isNeg)
         secs = -secs;
      size_t uidx = 0;
      while(0 < secs && secs < 1) {
         if (++uidx >= numofele(cstrUnits)) {
            --uidx;
            break;
         }
         secs *= 1000;
      }
      this->Span_ = isNeg ? (-secs) : secs;
      this->UnitStr_ = cstrUnits[uidx];
   }
   
   friend std::ostream& operator<<(std::ostream& os, const AutoTimeUnit& tu) {
      os << std::setw(static_cast<int>(os.precision()) + 4) << tu.Span_ << " " << tu.UnitStr_;
      return os;
   }
};

class StopWatch {
   using Clock = std::chrono::high_resolution_clock;
   Clock::time_point StartTime_;
public:
   StopWatch() {
      this->ResetTimer();
   }

   void ResetTimer() {
      this->StartTime_ = Clock::now();
   }

   template <class Period = std::ratio<1>>
   double StopTimer() {
      using Span = std::chrono::duration<double, Period>;
      Span  span = std::chrono::duration_cast<Span>(Clock::now() - this->StartTime_);
      this->ResetTimer();
      return span.count();
   }

   void PrintResult(const char* msg, uint64_t timesRun) {
      double span = this->StopTimer();
      std::cout << msg << ": "
         << span << " secs / " << timesRun << " times = "
         << AutoTimeUnit{span / static_cast<double>(timesRun)} << std::endl;
   }
};
} // namespace
#endif//__fon9_TestTools_hpp__
