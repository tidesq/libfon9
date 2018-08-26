// \file fon9/web/HttpDate.cpp
// \author fonwinz@gmail.com
#include "fon9/web/HttpDate.hpp"

namespace fon9 { namespace web {

static const char kDayName[][6] = {"Sun, ","Mon, ","Tue, ","Wed, ","Thu, ","Fri, ","Sat, "};
static const char kMonthName[][6] = {" Jan "," Feb "," Mar "," Apr "," May "," Jun ",
                                     " Jul "," Aug "," Sep "," Oct "," Nov "," Dec "};
fon9_API char* ToHttpDateRev(char* pout, TimeStamp ts) {
   // IMF-fixdate = day-name "," SP dd SP month SP yyyy SP HH:MM:SS SP GMT
   // "Sun, 06 Nov 1994 08:49:37 GMT"
   const struct tm& tm = GetDateTimeInfo(ts);
   if (static_cast<unsigned>(tm.tm_wday >= 7) || static_cast<unsigned>(tm.tm_mon) >= 12)
      return pout;
   memcpy(pout -= 4, " GMT", 4);
   pout = Pic9ToStrRev<2>(pout, static_cast<uint8_t>(tm.tm_sec));
   *--pout = ':';
   pout = Pic9ToStrRev<2>(pout, static_cast<uint8_t>(tm.tm_min));
   *--pout = ':';
   pout = Pic9ToStrRev<2>(pout, static_cast<uint8_t>(tm.tm_hour));
   *--pout = ' ';

   pout = Pic9ToStrRev<4>(pout, static_cast<uint16_t>(tm.tm_year + 1900));
   memcpy(pout -= 5, kMonthName[tm.tm_mon], 5);
   pout = Pic9ToStrRev<2>(pout, static_cast<uint8_t>(tm.tm_mday));

   memcpy(pout -= 5, kDayName[tm.tm_wday], 5);
   return pout;
}

fon9_API TimeStamp HttpDateTo(StrView str) {
   if (StrTrim(&str).size() < kHttpDateWidth)
      return TimeStamp{};
   struct tm tm;
   // "Sun, 06 Nov 1994 08:49:37 GMT"
   // 不理會 "Sun, "
   const char* pstr = str.begin() + 5;
   tm.tm_mday = (pstr[0] - '0') * 10 + (pstr[1] - '0');
   pstr += 2;
   tm.tm_mon = 0;
   for (const char* mon : kMonthName) {
      if (memcmp(mon, pstr, 5) == 0)
         break;
      ++tm.tm_mon;
   }
   pstr += 5;
   tm.tm_year = (pstr[0] - '0') * 1000
              + (pstr[1] - '0') * 100
              + (pstr[2] - '0') * 10
              + (pstr[3] - '0')
              - 1900;
   pstr += 5;
   tm.tm_hour = (pstr[0] - '0') * 10 + (pstr[1] - '0');
   pstr += 3;
   tm.tm_min = (pstr[0] - '0') * 10 + (pstr[1] - '0');
   pstr += 3;
   tm.tm_sec = (pstr[0] - '0') * 10 + (pstr[1] - '0');
   return TimeStamp{tm};
}

} } // namespace
