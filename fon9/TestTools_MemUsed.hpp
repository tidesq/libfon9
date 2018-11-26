/// \file fon9/TestTools_MemUsed.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_TestTools_MemUsed_hpp__
#define __fon9_TestTools_MemUsed_hpp__
#include "fon9/StrTo.hpp"

#ifdef fon9_WINDOWS
fon9_BEFORE_INCLUDE_STD;
#include <psapi.h> // for memory usage.
fon9_AFTER_INCLUDE_STD;
#endif

static uint64_t GetMemUsed() {
#ifdef fon9_WINDOWS
   PROCESS_MEMORY_COUNTERS_EX pmc;
   GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc));
   return pmc.PrivateUsage / 1024;
#else
   struct Aux {
      static uint64_t parseLine(char* line) {
         // This assumes that a digit will be found and the line ends in " Kb".
         const char* p = line;
         while (*p <'0' || *p > '9')
            ++p;
         return fon9::NaiveStrToUInt(fon9::StrView{p, line + strlen(line)});
      }
      static uint64_t getValue() { //Note: this value is in KB!
         FILE* file = fopen("/proc/self/status", "r");
         char line[128];
         while (fgets(line, 128, file) != NULL) {
            if (memcmp(line, "VmSize:", 7) == 0)
               return parseLine(line);
         }
         fclose(file);
         return 0;
      }
   };
   return Aux::getValue();
#endif
}

#endif//__fon9_TestTools_MemUsed_hpp__
