// \file fon9/Base64.cpp
// \author fonwinz@gmail.com
#include "fon9/Base64.hpp"
#include "fon9/StrTools.hpp"

namespace fon9 {

static const char chB64Enc[64] = {
   'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
   'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
   'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',
   'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
   'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x',
   'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7',
   '8', '9', '+', '/',
};

fon9_API Base64Result Base64Encode(char* dst, size_t dstSize, const void* src, size_t srcSize) {
   if (fon9_UNLIKELY(srcSize == 0)) {
      if (dstSize > 0)
         dst[0] = 0;
      return Base64Result{0};
   }
   const size_t srcM3 = srcSize % 3;
   const size_t minOutSize = (srcSize / 3 + (srcM3 != 0)) * 4;
   if (fon9_UNLIKELY(dstSize < minOutSize))
      return Base64Result{std::errc::no_buffer_space};

   const size_t n3 = srcSize - srcM3;
   size_t L;
   byte   C1, C2, C3;
   for (L = 0; L < n3; L += 3) {
      C1 = reinterpret_cast<const byte*>(src)[0];
      C2 = reinterpret_cast<const byte*>(src)[1];
      C3 = reinterpret_cast<const byte*>(src)[2];
      src = reinterpret_cast<const byte*>(src) + 3;

      *dst++ = chB64Enc[(C1 >> 2) & 0x3F];
      *dst++ = chB64Enc[(((C1 & 3) << 4) + (C2 >> 4)) & 0x3F];
      *dst++ = chB64Enc[(((C2 & 15) << 2) + (C3 >> 6)) & 0x3F];
      *dst++ = chB64Enc[C3 & 0x3F];
   }
   if (L < srcSize) {
      C1 = reinterpret_cast<const byte*>(src)[0];
      *dst++ = chB64Enc[(C1 >> 2) & 0x3F];
      if ((L + 1) < srcSize) {
         C2 = reinterpret_cast<const byte*>(src)[1];
         *dst++ = chB64Enc[(((C1 & 3) << 4) + (C2 >> 4)) & 0x3F];
         *dst++ = chB64Enc[((C2 & 15) << 2) & 0x3F];
      }
      else {
         *dst++ = chB64Enc[((C1 & 3) << 4) & 0x3F];
         *dst++ = '=';
      }
      *dst++ = '=';
   }
   if (dstSize > minOutSize)
      *dst = 0;
   return Base64Result{minOutSize};
}

#define E64    127
static const byte chB64Dec[128] = {
   E64, E64, E64, E64, E64, E64, E64, E64, E64, E64, E64, E64, E64, E64, E64, E64,
   E64, E64, E64, E64, E64, E64, E64, E64, E64, E64, E64, E64, E64, E64, E64, E64,
   E64, E64, E64, E64, E64, E64, E64, E64, E64, E64, E64,  62, E64, E64, E64,  63,
    52,  53,  54,  55,  56,  57,  58,  59,  60,  61, E64, E64, E64,  64, E64, E64,
   E64,   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,
    15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25, E64, E64, E64, E64, E64,
   E64,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,
    41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51, E64, E64, E64, E64, E64,
};
fon9_API Base64Result Base64DecodeLength(const char* srcB64, size_t srcSize) {
   const char* const srcEnd = srcB64 + srcSize;
   size_t dstSize = 0;
   size_t tailCount = 0;
   while (srcB64 < srcEnd) {
      byte chB64 = static_cast<byte>(*srcB64++);
      if (isspace(chB64))
         continue;
      if (fon9_UNLIKELY(chB64 >= sizeof(chB64Dec)))
         return Base64Result{std::errc::bad_message};
      if ((chB64 = chB64Dec[chB64]) == E64)
         return Base64Result{std::errc::bad_message};
      if (fon9_LIKELY(chB64 < 64)) { // 一般 B64 字元.
         if (tailCount != 0)         // 但是已有發現結束字元.
            return Base64Result{std::errc::bad_message}; // => 錯誤的字元.
      } else {                // 結束字元.
         if (++tailCount > 2) // 不可超過2個結束字元.
            return Base64Result{std::errc::bad_message};
      }
      ++dstSize;
   }
   return Base64Result{Base64DecodeLength(dstSize) - tailCount};
}

fon9_API Base64Result Base64Decode(void* dst, size_t dstSize, const char* srcB64, size_t srcSize) {
   byte* pout = reinterpret_cast<byte*>(dst);
   byte* poutEnd = pout + dstSize;
   const char* const srcEnd = srcB64 + srcSize;
   uint32_t tails = 3;
   uint32_t value = 0;
   uint32_t count = 0;
   while (srcB64 < srcEnd) {
      byte chB64 = static_cast<byte>(*srcB64++);
      if (isspace(chB64))
         continue;
      if (fon9_UNLIKELY(chB64 >= sizeof(chB64Dec)))
         return Base64Result{std::errc::bad_message};
      if ((chB64 = chB64Dec[chB64]) == E64)
         return Base64Result{std::errc::bad_message};
      if (fon9_LIKELY(chB64 < 64)) {
         if (fon9_UNLIKELY(tails != 3))
            return Base64Result{std::errc::bad_message};
         value = (value << 6) | (chB64 & 0x3F);
      }
      else {
         if (fon9_UNLIKELY(--tails == 0))
            return Base64Result{std::errc::bad_message};
         value <<= 6;
      }
      if (++count == 4) {
         if (pout + tails > poutEnd)
            return Base64Result{std::errc::no_buffer_space};
         count = 0;
         if (tails > 0) *pout++ = static_cast<byte>(value >> 16);
         if (tails > 1) *pout++ = static_cast<byte>(value >> 8);
         if (tails > 2) *pout++ = static_cast<byte>(value);
      }
   }
   if (count)
      return Base64Result{std::errc::bad_message};
   return Base64Result{static_cast<size_t>(pout - reinterpret_cast<byte*>(dst))};
}

} // namespaces
