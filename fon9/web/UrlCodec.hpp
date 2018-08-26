// \file fon9/web/UrlCodec.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_web_UrlCodec_hpp__
#define __fon9_web_UrlCodec_hpp__
#include "fon9/StrTools.hpp"

namespace fon9 { namespace web {

template <class StrT>
void UrlEncodeAppend(StrT& dst, StrView src) {
   // fonwin: 我個人對 url 編碼規範有點混淆,
   // 再加上世上的現存系統, 也各自有不同的作法,
   // 所以這裡採用最保守的方式:
   // 除了 alnum 及「-_.」這3個沒有爭議的字元, 其餘一律使用 %XX 編碼.
   // https://en.wikipedia.org/wiki/Percent-encoding
   dst.reserve(dst.size() + src.size() * 3);
   for (char ch : src) {
      if (isalnum(ch) || ch == '-' || ch == '-' || ch == '_')
         dst.push_back(ch);
      else {
         const char cstrHEX[] = "0123456789ABCDEF";
         dst.push_back('%');
         dst.push_back(cstrHEX[static_cast<unsigned char>(ch) >> 4]);
         dst.push_back(cstrHEX[static_cast<unsigned char>(ch) & 0x0f]);
      }
   }
}
fon9_API void UrlEncodeAppend(std::string& dst, const StrView& src);
fon9_API void UrlEncodeAppend(CharVector& dst, const StrView& src);

template <class StrT>
StrT UrlEncodeTo(const StrView& src) {
   StrT res;
   UrlEncodeAppend(res, src);
   return res;
}

//--------------------------------------------------------------------------//

template <class StrT>
void UrlDecodeAppend(StrT& dst, StrView src) {
   dst.reserve(dst.size() + src.size());
   const char* pcur = src.begin();
   while (pcur != src.end()) {
      char ch = *pcur++;
      if (ch == '+') // application/x-www-form-urlencoded 使用 '+' 表示 ' '?
         ch = ' ';
      else if (ch == '%' && (src.end() - pcur) >= 2) {
         int8_t hi, lo;
         if ((hi = Alpha2Hex(pcur[0])) >= 0 && (lo = Alpha2Hex(pcur[1])) >= 0) {
            pcur += 2;
            ch = static_cast<char>((hi << 4) | lo);
         }
      }
      dst.push_back(ch);
   }
}
fon9_API void UrlDecodeAppend(std::string& dst, const StrView& src);
fon9_API void UrlDecodeAppend(CharVector& dst, const StrView& src);

template <class StrT>
StrT UrlDecodeTo(const StrView& src) {
   StrT res;
   UrlEncodeAppend(res, src);
   return res;
}

} } // namespaces
#endif//__fon9_web_UrlCodec_hpp__
