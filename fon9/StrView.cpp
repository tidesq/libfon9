// \file fonix/StrArg.cpp
// \author fonixcpp@gmail.com
#include "fonix/StrArg.hpp"
namespace fonix {

#ifdef fonix_MEMRCHR
fonix_API const void* memrchr(const void* s, int c, size_t n) {
   if (n != 0) {
      const unsigned char* cp = reinterpret_cast<const unsigned char *>(s) + n;
      do {
         if (*(--cp) == static_cast<unsigned char>(c))
            return cp;
      } while (--n > 0);
   }
   return nullptr;
}
fonix_API const void* memrmem(const void* v, size_t size, const void *pat, size_t patSize) {
   assert(v != nullptr);
   assert(pat != nullptr);
   if (size < patSize)
      return nullptr;
   if (patSize == 0)
      return v;

   const char* p = reinterpret_cast<const char*>(v) + size - patSize;
   do {
      if (fonix_UNLIKELY(*p == *reinterpret_cast<const char*>(pat))) {
         for (size_t L = 1; L < patSize; ++L) {
            if (p[L] != reinterpret_cast<const char*>(pat)[L])
               goto __CHECK_PREV;
         }
         return p;
      }
   __CHECK_PREV:;
   } while (v != p--);
   return nullptr;
}
#endif

char* StrArg::ToString(char* vbuf, size_t vbufSize) const {
   size_t len = size();
   if (len >= vbufSize)
      len = vbufSize - 1;
   if (fonix_LIKELY(vbuf != Begin_))
      memmove(vbuf, Begin_, len);
   vbuf[len] = 0;
   return vbuf;
}

StrArg StrArg_TruncUTF8(const StrArg& utf8str, size_t expectLen) {
   if (utf8str.size() <= expectLen)
      return utf8str;
// 根據 wiki 的說明: https://en.wikipedia.org/wiki/UTF-8
// Bits of    | First       | Last         | Bytes in |
// code point | code point  | code point   | sequence | Byte 1   | Byte 2   | Byte 3   | Byte 4   | Byte 5   | Byte 6
//     7      | U + 0000    | U + 007F     | 1        | 0xxxxxxx |
//    11      | U + 0080    | U + 07FF     | 2        | 110xxxxx | 10xxxxxx |
//    16      | U + 0800    | U + FFFF     | 3        | 1110xxxx | 10xxxxxx | 10xxxxxx |
//    21      | U + 10000   | U + 1FFFFF   | 4        | 11110xxx | 10xxxxxx | 10xxxxxx | 10xxxxxx |
//    26      | U + 200000  | U + 3FFFFFF  | 5        | 111110xx | 10xxxxxx | 10xxxxxx | 10xxxxxx | 10xxxxxx |
//    31      | U + 4000000 | U + 7FFFFFFF | 6        | 1111110x | 10xxxxxx | 10xxxxxx | 10xxxxxx | 10xxxxxx | 10xxxxxx
// 所以根據被截斷的地方來判斷即可!
   static const char chHead = '\x80' | 0x40;
   const char* pbegin = utf8str.begin();
   const char* ptrunc = pbegin + expectLen;
   // 依上表來看, 此處最多走6步就可完成!
   while ((*ptrunc & chHead) != chHead) {
      if ((*ptrunc & 0x80) == 0)
         break;
      if (--ptrunc == pbegin)
         break;
   }
   return StrArg{pbegin, ptrunc, StrNoEOS};
}

} // namespaces
