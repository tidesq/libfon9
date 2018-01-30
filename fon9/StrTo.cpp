/// \file fon9/StrTo.cpp
/// \author fonwinz@gmail.com
#include "fon9/StrTo.hpp"

namespace fon9 {

fon9_API uintmax_t HexStrTo(StrView hexstr, const char** endptr) {
   StrTrimHead(hexstr);
   const char ch1 = static_cast<char>(hexstr.Get1st());
   if (ch1 == 'x' || ch1 == 'X')
      hexstr.SetBegin(hexstr.begin() + 1);
   uintmax_t  value = 0;
   for (const char& ch : hexstr) {
      int8_t i = Alpha2Hex(ch);
      if (fon9_UNLIKELY(i < 0)) {
         if (endptr)
            *endptr = &ch;
         break;
      }
      value = (value << 4) | static_cast<uint8_t>(i);
   }
   return value;
}
fon9_API uintmax_t HIntStrTo(StrView str, const char** endptr) {
   StrTrimHead(str);
   char ch = static_cast<char>(str.Get1st());
   if (ch == '0') {
      str.SetBegin(str.begin() + 1);
      ch = static_cast<char>(str.Get1st());
   }
   if (ch == 'x' || ch == 'X')
      return HexStrTo(str, endptr);
   return NaiveStrToUInt(str, endptr);
}

fon9_API intmax_t NaiveStrToSInt(const char *pbeg, const char* pend, const char** endptr) {
   if (fon9_UNLIKELY(pbeg >= pend)) {
      if (endptr)
         *endptr = pend;
      return 0;
   }
   bool isNeg;
   switch (*pbeg) {
   case '-':
      ++pbeg;
      isNeg = true;
      break;
   case '+':
      ++pbeg;
   default:
      isNeg = false;
      break;
   }
   using ResultT = intmax_t;
   ResultT  res = static_cast<ResultT>(NaiveStrToUInt(pbeg, pend, endptr));
   return (isNeg ? -res : res);
}
fon9_API uintmax_t NaiveStrToUInt(const char *pbeg, const char* pend, const char** endptr) {
   using ResultT = uintmax_t;
   ResultT  res = 0;
   while (pbeg < pend) {
      unsigned char digval = static_cast<unsigned char>(*pbeg - '0');
      if (digval > 9)
         break;
      res = (res * 10) + digval;
      ++pbeg;
   }
   if (endptr)
      *endptr = pbeg;
   return res;
}

} // namespaces
