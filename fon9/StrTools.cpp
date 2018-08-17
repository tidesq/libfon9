// \file fon9/StrTools.cpp
// \author fonwinz@gmail.com
#include "fon9/StrTools.hpp"
#include <algorithm> // std::search()

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

//--------------------------------------------------------------------------//

fon9_API StrView StrSplit(StrView& src, char chDelim) {
   if (const char* pEqual = StrView::traits_type::find(src.begin(), src.size(), chDelim)) {
      StrView retval{src.begin(), StrFindTrimTail(src.begin(), pEqual)};
      src.Reset(StrFindTrimHead(pEqual + 1, src.end()), src.end());
      return retval;
   }
   StrView retval{src};
   src.Reset(nullptr);
   return retval;
}

fon9_API bool StrFetchTagValue(StrView& src, StrView& tag, StrView& value, char chFieldDelim, char chEqual) {
   if (StrTrimHead(&src).empty())
      return false;
   value = StrFetchTrim(src, chFieldDelim);
   tag = StrSplit(value, chEqual);
   return true;
}

//--------------------------------------------------------------------------//

static const StrBrPair DefaultBrPair[]{
   {'{', '}', true},
   {'[', ']', true},
   {'(', ')', true},
   {'\'', '\'', false},
   {'"', '"', false},
   {'`', '`', false},
};
const StrBrArg StrBrArg::Default_{DefaultBrPair + 0, numofele(DefaultBrPair)}; // Default_{DefaultBrPair}; VC 會有 Warn C4592.
const StrBrArg StrBrArg::DefaultNoCurly_{DefaultBrPair + 1, numofele(DefaultBrPair) - 1};
const StrBrArg StrBrArg::Quotation_{DefaultBrPair + 3, 3};

const StrBrPair* StrBrArg::Find(char chLeft) const {
   if (this->BrCount_ <= 0)
      return nullptr;
   const StrBrPair* beg = this->BrBegin_;
   const StrBrPair* end = beg + this->BrCount_;
   for (; beg != end; ++beg) {
      if (beg->Left_ == chLeft)
         return beg;
   }
   return nullptr;
}

//--------------------------------------------------------------------------//

//
// 引號「'、"」內可為「除了 chEndQ」的任意字元(除非 chEndQ 前面是 '\'), 包含沒對應的括號.
static const char* StrSkipQuotationEnd(const char* pbeg, const char* pend, char chEndQ) {
   while (const char* pbrEnd = StrView::traits_type::find(pbeg, static_cast<size_t>(pend - pbeg), chEndQ)) {
      if (pbrEnd == pbeg || *(pbrEnd - 1) != '\\')
         return pbrEnd;
      pbeg = pbrEnd + 1;
   }
   return nullptr;
}

static const char* SbrFindSplit(const char* pbeg, const char* pend, char chDelim, const StrBrArg& brArg);
// *(pbeg-1) 必定 == br.Left_; 然後尋找結束的引號, 或對應的右括號.
inline static const char* SbrFindEnd(const StrBrPair& br, const char* pbeg, const char* pend, const StrBrArg& brArg) {
   return br.IsAllowNest_
      ? SbrFindSplit(pbeg, pend, br.Right_, brArg)
      : StrSkipQuotationEnd(pbeg, pend, br.Right_);
}
static const char* SbrFindSplit(const char* pbeg, const char* pend, char chDelim, const StrBrArg& brArg) {
   while (pbeg < pend) {
      const char  chBeg = *pbeg;
      if (chBeg == chDelim)
         return pbeg;
      ++pbeg;
      if (chBeg == '\\') {
         ++pbeg;
         continue;
      }
      if (const StrBrPair* br = brArg.Find(chBeg))
         if (const char* pspl = SbrFindEnd(*br, pbeg, pend, brArg))
            pbeg = pspl + 1;
   }
   return nullptr;
}

fon9_API StrView SbrFetchFieldNoTrim(StrView& src, char chDelim, const StrBrArg& brArg) {
   if (const char* pDelim = SbrFindSplit(src.begin(), src.end(), chDelim, brArg)) {
      StrView retval{src.begin(), pDelim};
      src.SetBegin(pDelim + 1);
      return retval;
   }
   StrView retval{src};
   src.SetBegin(src.end());
   return retval;
}

fon9_API StrView SbrFetchInsideNoTrim(StrView& src, const StrBrArg& brArg) {
   const char* pbeg = src.begin();
   if (const StrBrPair* br = brArg.Find(*pbeg)) {
      ++pbeg;
      const char* pBrEnd = SbrFindEnd(*br, pbeg, src.end(), brArg);
      if (pBrEnd)
         src.SetBegin(pBrEnd + 1);
      else
         src.SetBegin(pBrEnd = src.end());
      return StrView{pbeg, pBrEnd};
   }
   return StrView{nullptr};
}

//--------------------------------------------------------------------------//

fon9_API void StrView_ToNormalizeStr(std::string& dst, StrView src) {
   const char* pbeg = src.begin();
   const char* pend = src.end();
   if (pbeg == pend)
      return;
   while (const char* pspl = StrView::traits_type::find(pbeg, static_cast<size_t>(pend - pbeg), '\\')) {
      if (pspl != pbeg) // 先把 '\\' 之前的字串放到 outstr.
         dst.append(pbeg, pspl);
      if (++pspl >= pend)// 若 '\\' 之後已到結尾, 則拋棄最後這個 '\\'
         break;
      switch (char ch = *pspl) {
      case 'a':   dst.push_back('\a'); break;
      case 'b':   dst.push_back('\b'); break;
      case 'f':   dst.push_back('\f'); break;
      case 'n':   dst.push_back('\n'); break;
      case 'r':   dst.push_back('\r'); break;
      case 't':   dst.push_back('\t'); break;
      case 'v':   dst.push_back('\v'); break;
      case 'x':   // xn 或 xnn: 一個 16 進位字元.
         ch = Alpha2Hex(*(++pspl));
         if (static_cast<uint8_t>(ch) <= 0x0f) {
            unsigned char uch2 = static_cast<unsigned char>(Alpha2Hex(*(++pspl)));
            if (uch2 <= 0x0f) {
               ch = static_cast<char>(ch << 4 | uch2);
               ++pspl;
            }
         }
         dst.push_back(ch);
         pbeg = pspl;
         continue;
      default:
         if (static_cast<unsigned char>(ch - '0') < 8) {
            // 指定數字的字元(與 C 的定義相同: 一律使用8進位).
            unsigned char uch2 = static_cast<unsigned char>(*++pspl - '0');
            if (uch2 < 8) {
               ch = static_cast<char>(ch << 3 | uch2);
               uch2 = static_cast<unsigned char>(*++pspl - '0');
               if (uch2 < 8) {
                  ch = static_cast<char>(ch << 3 | uch2);
                  ++pspl;
               }
            }
            dst.push_back(ch);
            pbeg = pspl;
            continue;
         }
         // 不認識的 '\?', 直接把 '?' 放進 dst.
         dst.push_back(ch);
         break;
      }
      pbeg = pspl + 1;
   }
   dst.append(pbeg, pend);
}

fon9_API void StrView_ToEscapeStr(std::string& dst, StrView src, StrView chSpecials) {
   const char* pbeg = src.begin();
   const char* pend = src.end();
   size_t      szSpecials = chSpecials.size();
   while (pbeg != pend) {
      char ch = *pbeg;
      switch (ch) {
      case '\\':
      case '\'':
      case '\"':  dst.push_back('\\'); break;
      case '\a':  dst.push_back('\\'); ch = 'a'; break;//0x07, audible bell.
      case '\b':  dst.push_back('\\'); ch = 'b'; break;//0x08, backspace
      case '\f':  dst.push_back('\\'); ch = 'f'; break;//0x0c, form feed - new page
      case '\n':  dst.push_back('\\'); ch = 'n'; break;//0x0a, line feed - new line
      case '\r':  dst.push_back('\\'); ch = 'r'; break;//0x0d, carriage return
      case '\t':  dst.push_back('\\'); ch = 't'; break;//0x09, horizontal tab
      case '\v':  dst.push_back('\\'); ch = 'v'; break;//0x0b, vertical tab
      default:
         if (fon9_UNLIKELY(static_cast<unsigned char>(ch) <= '\x1f')) {
            // "\xHH", 為了避免 "xH" 之後的字元可能剛好是 'a..f', 所以這裡強制使用2碼HH輸出.
            dst.append((ch & 0xf0) ? "\\x1" : "\\x0", 3);
            ch = "0123456789abcdef"[ch & 0x0f];
         }
         else if (fon9_UNLIKELY(szSpecials > 0)
                  && fon9_UNLIKELY(StrView::traits_type::find(chSpecials.begin(), szSpecials, ch)))
            dst.push_back('\\');
         break;
      }
      dst.push_back(ch);
      ++pbeg;
   }
}

//--------------------------------------------------------------------------//

fon9_API const char* StrSearchSubstr(StrView fullstr, StrView substr, char chSplitter) {
   const char* pfind = std::search(fullstr.begin(), fullstr.end(), substr.begin(), substr.end());
   if (pfind == fullstr.end())
      return nullptr;
   if (pfind == fullstr.begin() || *(pfind - 1) == chSplitter) {
      const char* pend = pfind + substr.size();
      if (pend == fullstr.end() || *pend == chSplitter)
         return pfind;
   }
   return nullptr;
}
fon9_API std::string StdStrReplace(StrView src, const StrView oldStr, const StrView newStr) {
   return StrReplaceImpl<std::string>(src, oldStr, newStr);
}
fon9_API CharVector CharVectorReplace(StrView src, const StrView oldStr, const StrView newStr) {
   return StrReplaceImpl<CharVector>(src, oldStr, newStr);
}

} // namespace fon9
