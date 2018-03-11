// \file fon9/StrTools.cpp
// \author fonwinz@gmail.com
#include "fon9/StrTools.hpp"

namespace fon9 {

const NumPunct         NumPunct::Default = {'.', ',',{'\3'}};
thread_local NumPunct  NumPunct_Current = NumPunct::Default;
NumPunct& NumPunct::GetCurrent() {
   return NumPunct_Current;
}

const char  Seq2AlphaMap[62] = {
   '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
   'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
   'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
   'U', 'V', 'W', 'X', 'Y', 'Z',
   'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
   'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
   'u', 'v', 'w', 'x', 'y', 'z'
};

#define _1     static_cast<int8_t>(-1)
const int8_t   Alpha2SeqMap128[128] = {
//  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  a,  b,  c,  d,  e,  f,
   _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1,   // 0x00..0x0f
   _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1,   // 0x10..0x1f
   _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1,   // 0x20..0x2f
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, _1, _1, _1, _1, _1, _1,   // 0x30..0x3f
   _1, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,   // 0x40..0x4f
   25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, _1, _1, _1, _1, _1,   // 0x50..0x5f
   _1, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,   // 0x60..0x6f
   51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, _1, _1, _1, _1, _1,   // 0x70..0x7f
};

const int8_t   Alpha2HexMap128[128] = {
//  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  a,  b,  c,  d,  e,  f,
   _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1,   // 0x00..0x0f
   _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1,   // 0x10..0x1f
   _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1,   // 0x20..0x2f
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, _1, _1, _1, _1, _1, _1,   // 0x30..0x3f
   _1, 10, 11, 12, 13, 14, 15, _1, _1, _1, _1, _1, _1, _1, _1, _1,   // 0x40..0x4f
   _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1,   // 0x50..0x5f
   _1, 10, 11, 12, 13, 14, 15, _1, _1, _1, _1, _1, _1, _1, _1, _1,   // 0x60..0x6f
   _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1, _1,   // 0x70..0x7f
};

const char Digs0099[] =
"0001020304050607080910111213141516171819"
"2021222324252627282930313233343536373839"
"4041424344454647484950515253545556575859"
"6061626364656667686970717273747576777879"
"8081828384858687888990919293949596979899";

//--------------------------------------------------------------------------//

fon9_API StrView StrView_TruncUTF8(StrView utf8str, size_t expectLen) {
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
   const char* const pbegin = utf8str.begin();
   const char* ptrunc = pbegin + expectLen;
   // 依上表來看, 此處最多走6步就可完成!
   while (ptrunc != pbegin && (*ptrunc & chHead) != chHead) {
      if ((*ptrunc & 0x80) == 0)
         break;
      --ptrunc;
   }
   return StrView{pbegin, ptrunc};
}
} // namespace fon9
