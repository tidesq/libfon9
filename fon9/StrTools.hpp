/// \file fon9/StrTools.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_StrTools_hpp__
#define __fon9_StrTools_hpp__
#include "fon9/StrView.hpp"
#include "fon9/Utility.hpp"

namespace fon9 {

/// \ingroup AlNum
/// 數字相關符號字元、參數, 請參閱 std::numpunct
struct fon9_API NumPunct {
   /// 小數點符號, 例:'.'
   char  DecPoint_;
   /// 千位分隔符號, 例:','
   char  IntSep_;
   /// 千位分組長度, 例:{'\3'}.
   unsigned char  Grouping_[14];
   /// 預設: DecPoint_='.';  IntSep_=',';  Grouping_={'\3'};
   static const NumPunct   Default;
   static NumPunct& GetCurrent();
};
/// 提供給 fon9 內部直接使用.
/// VC 無法在 thread_local 加上 fon9_API,
/// 所以 libfon9.dll 的使用者, 請用: NumPunct::GetCurrent();
extern thread_local NumPunct   NumPunct_Current;

//--------------------------------------------------------------------------//

/// \ingroup AlNum
/// 序號(0..61) => ASCII(UTF8)字元('0'..'9', 'A'..'Z', 'a'..'z') 的對照表.
extern fon9_API const char Seq2AlphaMap[62];

enum {
   /// \ingroup AlNum
   /// 可用一位「數字字元、文字字元」直接表達的數字數量('0'..'9', 'A'..'Z', 'a'..'z' 共62個)
   kSeq2AlphaSize = sizeof(Seq2AlphaMap)
};

/// \ingroup AlNum
/// 用 0..61 的數字取得 ASCII(UTF8) '0'..'9', 'A'..'Z', 'a'..'z' 的字元.
/// 若 seq >= fon9::kSeq2AlphaSize 則無法保證取得的值!
inline char Seq2Alpha(uint8_t seq) {
   assert(seq < kSeq2AlphaSize);
   return Seq2AlphaMap[seq];
}

/// \ingroup AlNum
/// ASCII(UTF8)字元('0'..'9', 'A'..'Z', 'a'..'z') => 序號(0..61) 的對照表.
extern fon9_API const int8_t  Alpha2SeqMap128[128];
/// \ingroup AlNum
/// ASCII(UTF8)字元('0'..'9', 'A'..'F', 'a'..'f') => 序號(0..15) 的對照表.
extern fon9_API const int8_t  Alpha2HexMap128[128];

/// \ingroup AlNum
/// 用 ASCII(UTF8) char '0'..'9', 'A'..'Z', 'a'..'z' 取得 0..61 的數字.
/// 若非以上字元, 則傳回 -1.
inline int8_t Alpha2Seq(char ch) {
   return static_cast<unsigned char>(ch) >= numofele(Alpha2SeqMap128)
      ? -1 : Alpha2SeqMap128[static_cast<unsigned char>(ch)];
}
/// \ingroup AlNum
/// 用 ASCII(UTF8) char '0'..'9', 'A'..'F' or 'a'..'f' 取得 0..15 的數字.
/// 若非以上字元, 則傳回 -1.
inline int8_t Alpha2Hex(char ch) {
   return static_cast<unsigned char>(ch) >= numofele(Alpha2HexMap128)
      ? -1 : Alpha2HexMap128[static_cast<unsigned char>(ch)];
}

/// \ingroup AlNum
// "00010203" ... "99"
extern fon9_API const char Digs0099[100 * 2 + 1];
inline char* Put2Digs(char* pout, uint8_t i00_99) {
   const char* const dig2 = Digs0099 + (i00_99 << 1);
   *--pout = dig2[1];
   *--pout = dig2[0];
   return pout;
}

//--------------------------------------------------------------------------//

constexpr bool iscntrl(int ch) { return static_cast<unsigned>(ch) <= '\x1f' || ch == '\x7f'; }
constexpr bool isprint(int ch) { return '\x20' <= ch && ch <= '\x7e'; }
constexpr bool isspace(int ch) { return '\x20' == ch || ('\x9' <= ch && ch <= '\xd'); }
constexpr bool isblank(int ch) { return '\x20' == ch || ch == '\t'; }
constexpr bool isgraph(int ch) { return '\x21' <= ch && ch < '\x7f'; }
constexpr bool isalnum(int ch) { return static_cast<unsigned>(ch) >= numofele(Alpha2SeqMap128) ? false : (Alpha2SeqMap128[ch] >= 0); }
constexpr bool isupper(int ch) { return 'A' <= ch && ch <= 'Z'; }
constexpr bool islower(int ch) { return 'a' <= ch && ch <= 'z'; }
constexpr bool isalpha(int ch) { return (static_cast<unsigned>(ch | 32) - 97) < 26u; }
constexpr bool isdigit(int ch) { return static_cast<unsigned>(ch - '0') <= 9; }

constexpr bool isnotcntrl(int ch) { return !iscntrl(ch); }
constexpr bool isnotprint(int ch) { return !isprint(ch); }
constexpr bool isnotspace(int ch) { return !isspace(ch); }
constexpr bool isnotblank(int ch) { return !isblank(ch); }
constexpr bool isnotgraph(int ch) { return !isgraph(ch); }
constexpr bool isnotalnum(int ch) { return !isalnum(ch); }
constexpr bool isnotupper(int ch) { return !isupper(ch); }
constexpr bool isnotlower(int ch) { return !islower(ch); }
constexpr bool isnotalpha(int ch) { return !isalpha(ch); }
constexpr bool isnotdigit(int ch) { return !isdigit(ch); }

//--------------------------------------------------------------------------//

/// \ingroup AlNum
/// 找到第一個 `FnPredicate(unsigned char ch)==true` 的字元, 如果沒找到則傳回 nullptr.
template <class FnPredicate>
inline const char* StrFindIf(const char* ibegin, const char* iend, FnPredicate fnPred) {
   for (; ibegin != iend; ++ibegin)
      if (fnPred(static_cast<unsigned char>(*ibegin)))
         return ibegin;
   return nullptr;
}

/// \ingroup AlNum
/// 找到第一個 `FnPredicate(unsigned char ch)==true` 的字元, 如果沒找到則傳回 nullptr.
template <class... FnPredicate>
constexpr const char* StrFindIf(const StrView& str, FnPredicate&&... fnPred) {
   return StrFindIf(str.begin(), str.end(), std::forward<FnPredicate>(fnPred)...);
}

//--------------------------------------------------------------------------//

/// \ingroup AlNum
/// 從尾端往前找, 找到第一次 `FnPredicate(unsigned char ch)==true` 的字元, 如果沒找到則傳回 nullptr.
template <class FnPredicate>
inline const char* StrRFindIf(const char* ibegin, const char* iend, FnPredicate fnPred) {
   while (ibegin != iend)
      if (fnPred(static_cast<unsigned char>(*--iend)))
         return iend;
   return nullptr;
}

/// \ingroup AlNum
/// 從尾端往前找, 找到第一次 `FnPredicate(unsigned char ch)==true` 的字元, 如果沒找到則傳回 nullptr.
template <class... FnPredicate>
constexpr const char* StrRFindIf(const StrView& str, FnPredicate&&... fnPred) {
   return StrRFindIf(str.begin(), str.end(), std::forward<FnPredicate>(fnPred)...);
}

//--------------------------------------------------------------------------//

/// \ingroup AlNum
/// 移除 str 前方的空白字元: `isspace()==true` 的字元.
/// 若 str 全都是空白, 則將 str 移動到尾端: str.SetBegin(str.end()).
inline void StrTrimHead(StrView& str) {
   const char* pbeg = StrFindIf(str, isnotspace);
   str.SetBegin(pbeg ? pbeg : str.end());
}
inline void StrTrimHead(StrView& str, const char* pbegin) {
   const char* pbeg = StrFindIf(pbegin, str.end(), isnotspace);
   str.SetBegin(pbeg ? pbeg : str.end());
}

/// \ingroup AlNum
/// 移除 str 尾端的空白字元: `isspace()==true` 的字元.
/// 若 str 全都是空白, 則將 str 移動到頭端: str.SetEnd(str.begin()).
inline void StrTrimTail(StrView& str) {
   const char* pend = StrRFindIf(str, isnotspace);
   str.SetEnd(pend ? pend : str.begin());
}

/// \ingroup AlNum
/// 移除 str 頭尾的空白字元: `isspace()==true` 的字元.
/// 若 str 全都是空白, 則將 str 移動到尾端: str.SetBegin(str.end()).
inline void StrTrim(StrView& str) {
   if (const char* pbeg = StrFindIf(str, isnotspace))
      str.Reset(pbeg, StrFindIf(pbeg, str.end(), isnotspace) + 1);
   else
      str.SetBegin(str.end());
}

//--------------------------------------------------------------------------//

/// \ingroup AlNum
/// 如果 utf8str 長度超過 expectLen 則切除超過的部分,
/// 如果切除的位置剛好是一個 [utf8字] 的一部份, 則長度會再縮減, 避免有被切斷的 [utf8字].
fon9_API StrView StrView_TruncUTF8(StrView utf8str, size_t expectLen);

} // namespace fon9
#endif//__fon9_StrTools_hpp__
