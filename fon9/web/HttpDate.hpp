// \file fon9/web/HttpDate.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_web_HttpDate_hpp__
#define __fon9_web_HttpDate_hpp__
#include "fon9/TimeStamp.hpp"

namespace fon9 { namespace web {

fon9_API TimeStamp HttpDateTo(StrView str);

enum : size_t {
   kHttpDateWidth = sizeof("Sun, 06 Nov 1994 08:49:37 GMT") -1,
};
/// day-name "," SP date1 SP time-of-day SP GMT
fon9_API char* ToHttpDateRev(char* pout, TimeStamp ts);

struct FmtHttpDate {
   TimeStamp TS_;
};
inline char* ToStrRev(char* pout, const FmtHttpDate& httpts) {
   return ToHttpDateRev(pout, httpts.TS_);
}

} } // namespaces
#endif//__fon9_web_HttpDate_hpp__
