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

fon9_API char* PutAutoPrecision(char* pout, uintmax_t& value, DecScaleT scale) {
   for (; scale > 0; --scale) {
      if (char ch = static_cast<char>(value % 10)) {
         for (;;) {
            *--pout = static_cast<char>(ch + '0');
            value /= 10;
            if (--scale <= 0)
               break;
            ch = static_cast<char>(value % 10);
         }
         // 自動小數位數 & 小數部分非0, 則一定要填小數點.
         *--pout = NumPunct_Current.DecPoint_;
         break;
      }
      // 排除尾端=0的小數.
      value /= 10;
   }
   return pout;
}

} // namespaces
