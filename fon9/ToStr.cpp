/// \file fon9/ToStr.cpp
/// \author fonwinz@gmail.com
#include "fon9/ToStr.hpp"

namespace fon9 {

fon9_API char* HexToStrRev(char* pout, uintmax_t value) {
   static const char map2Hchar[] = "0123456789abcdef";
   #define TO_HEX_CHAR(v)  
   do {
      *--pout = map2Hchar[static_cast<uint8_t>(value & 0x0f)];
      if ((value >>= 4) == 0)
         break;
      *--pout = map2Hchar[static_cast<uint8_t>(value & 0x0f)];
   } while ((value >>= 4) != 0);
   return pout;
   #undef TO_HEX_CHAR
}

fon9_API char* UIntToStrRev_IntSep(char* pout, uintmax_t value) {
   const unsigned char* pGrouping = NumPunct_Current.Grouping_;
   unsigned char        nsep = *pGrouping;
   if (fon9_UNLIKELY(nsep == 0))
      return UIntToStrRev(pout, value);

   const unsigned char* const pGroupingEND = NumPunct_Current.Grouping_ + sizeof(NumPunct_Current.Grouping_);
   unsigned char              iSepCount = 0;
   for (;;) {
      *--pout = static_cast<char>(static_cast<char>(value % 10) + '0');
      if ((value /= 10) == 0)
         break;
      if (fon9_LIKELY(++iSepCount != nsep))
         continue;
      iSepCount = 0;
      *--pout = NumPunct_Current.IntSep_;
      if (pGrouping + 1 < pGroupingEND) {
         if (unsigned char n = *++pGrouping)
            nsep = n;
         else
            pGrouping = pGroupingEND;
      }
   }
   return pout;
}

fon9_API char* PutAutoPrecision(char* pout, uintmax_t& value, DecScaleT scale) {
   uintmax_t res = value;
   for (; scale > 0; --scale) {
      if (char ch = static_cast<char>(res % 10)) {
         for (;;) {
            *--pout = static_cast<char>(ch + '0');
            res /= 10;
            if (--scale <= 0)
               break;
            ch = static_cast<char>(res % 10);
         }
         // 自動小數位數 & 小數部分非0, 則一定要填小數點.
         *--pout = NumPunct_Current.DecPoint_;
         break;
      }
      // 排除尾端=0的小數.
      res /= 10;
   }
   value = res;
   return pout;
}

} // namespaces
