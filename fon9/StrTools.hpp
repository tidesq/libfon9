/// \file fon9/StrTools.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_StrTools_hpp__
#define __fon9_StrTools_hpp__
#include "fon9/CharVector.hpp"
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
/// 從頭往後找第一個非空白的字元位置.
/// \retval pend  全都是空白.
/// \retval !pend 非空白字元位置.
inline const char* StrFindTrimHead(const char* pbeg, const char* pend) {
   if (const char* p = StrFindIf(pbeg, pend, isnotspace))
      return p;
   return pend;
}

/// \ingroup AlNum
/// - 移除 str 前方的空白字元: `isspace()==true` 的字元.
/// - 若 str 全都是空白, 則將 str 移動到尾端: str->SetBegin(str->end()).
/// - 為了讓介面清楚明示 str 會被修改, 所以使用 `StrView*`.
/// - 傳回值: reference of `*str`
inline StrView& StrTrimHead(StrView* str) {
   str->SetBegin(StrFindTrimHead(str->begin(), str->end()));
   return *str;
}
inline StrView& StrTrimHead(StrView* str, const char* pfrom) {
   assert(str->begin() <= pfrom && pfrom <= str->end());
   str->SetBegin(StrFindTrimHead(pfrom, str->end()));
   return *str;
}

/// \ingroup AlNum
/// 從尾往前找第一個非空白的字元位置.
/// \retval pbeg  全都是空白.
/// \retval pos   非空白字元位置+1.
inline const char* StrFindTrimTail(const char* pbeg, const char* pend) {
   if (const char* p = StrRFindIf(pbeg, pend, isnotspace))
      return p + 1;
   return pbeg;
}

/// \ingroup AlNum
/// - 移除 str 尾端的空白字元: `isspace()==true` 的字元.
/// - 若 str 全都是空白, 則將 str 移動到頭端: str->SetEnd(str->begin()).
/// - 為了讓介面清楚明示 str 會被修改, 所以使用 `StrView*`.
/// - 傳回值: reference of `*str`
inline StrView& StrTrimTail(StrView* str) {
   str->SetEnd(StrFindTrimTail(str->begin(), str->end()));
   return *str;
}

/// \ingroup AlNum
/// - 移除 str 頭尾的空白字元: `isspace()==true` 的字元.
/// - 若 str 全都是空白, 則將 str 移動到尾端: str.SetBegin(str.end()).
/// - 為了讓介面清楚明示 str 會被修改, 所以使用 `StrView*`.
/// - 傳回值: reference of `*str`
inline StrView& StrTrim(StrView* str) {
   if (const char* pbeg = StrFindIf(*str, isnotspace))
      str->Reset(pbeg, StrRFindIf(pbeg, str->end(), isnotspace) + 1);
   else
      str->SetBegin(str->end());
   return *str;
}

//--------------------------------------------------------------------------//

template <class TrimAux, class FnSplitter>
StrView StrFetch(StrView& src, TrimAux trimAux, FnSplitter fnSplitter) {
   (void)trimAux;
   const char* const pbeg = src.begin();
   const char* const pend = src.end();
   if (pbeg != pend) {
      const char* pDelim = fnSplitter(pbeg, pend);
      if (pDelim && pDelim != pend) {
         src.SetBegin(pDelim + 1);
         return StrView{pbeg, trimAux.TrimDelim(pbeg, pDelim)};
      }
      src.SetBegin(pend);
      return StrView{pbeg, trimAux.TrimTail(pbeg, pend)};
   }
   return StrView{pbeg, pbeg};
}

template <class FnSplitter>
StrView StrFetchNoTrim(StrView& src, FnSplitter fnSplitter) {
   struct TrimAux {
      static constexpr const char* TrimDelim(const char*, const char* pDelim) { return pDelim; }
      static constexpr const char* TrimTail(const char*, const char* pend) { return pend; }
   };
   return StrFetch(src, TrimAux{}, std::move(fnSplitter));
}

inline StrView StrFetchNoTrim(StrView& src, char chDelim) {
   return StrFetchNoTrim(src, [chDelim](const char* pbeg, const char* pend) {
      return StrView::traits_type::find(pbeg, static_cast<size_t>(pend - pbeg), chDelim);
   });
}

/// \ingroup AlNum
/// - 透過 fnSplitter 找到要分割的位置.
/// - 然後傳回分割位置前的字串(移除前後空白)
/// - src.begin() 移動到分割位置+1(沒有移除空白)
/// - 如果沒找到分割位置, 則傳回: 移除前後空白的src, src 移到 end() 的位置.
template <class FnSplitter>
StrView StrFetchTrim(StrView& src, FnSplitter fnSplitter) {
   struct TrimAux {
      static const char* TrimDelim(const char* pbeg, const char* pDelim) { return StrFindTrimTail(pbeg, pDelim); }
      // 因為一開頭已呼叫過 StrTrimHead() & 檢查 src.size() != 0;
      // 所以 src 必定有非空白字元, 此時 StrRFindIf() 必定不會是 nullptr.
      static const char* TrimTail(const char* pbeg, const char* pend) { return StrRFindIf(pbeg, pend, isnotspace) + 1; }
   };
   return StrFetch(StrTrimHead(&src), TrimAux{}, std::move(fnSplitter));
}

template <class FnPred>
struct StrCharSplitterFn {
   decay_t<FnPred> FnPred_;
   template <class... ArgsT>
   constexpr StrCharSplitterFn(ArgsT&&... args) : FnPred_{std::forward<ArgsT>(args)...} {
   }
   const char* operator()(const char* pbeg, const char* pend) {
      while (pbeg != pend) {
         if (this->FnPred_(static_cast<unsigned char>(*pbeg)))
            return pbeg;
         ++pbeg;
      }
      return pend;
   }
};

/// \ingroup AlNum
/// 建立一個尋找物件, 用來尋找 [pbeg,pend) 符合 fnPred() 的位置.
/// 使用字元判斷函式(e.g. &isspace, &isnotspace) 來建立該尋找物件.
template <class FnPred>
constexpr StrCharSplitterFn<FnPred> StrMakeCharSplitterFn(FnPred&& fnPred) {
   return StrCharSplitterFn<FnPred>{std::move(fnPred)};
}

/// \ingroup AlNum
/// 透過 CharSplitterFn<> 機制, 提供使用「字元函式(e.g. &isspace)」判斷分隔符號.
/// e.g. `StrView v = StrFetchTrim(src, &isspace);`
inline StrView StrFetchTrim(StrView& src, bool (*fn)(int ch)) {
   return StrFetchTrim(src, StrMakeCharSplitterFn(fn));
}

/// \ingroup AlNum
/// - 找到第一個 chDelim = 要分割的位置.
/// - 如果有找到 chDelim:
///   - 傳回分割位置前的字串(移除前後空白)
///   - src.begin() 移動到分割位置+1(沒有移除空白)
/// - 如果沒找到 chDelim, 則傳回: 移除前後空白的src, src.begin() 移到 end() 的位置.
inline StrView StrFetchTrim(StrView& src, char chDelim) {
   return StrFetchTrim(src, [chDelim](const char* pbeg, const char* pend) {
      return StrView::traits_type::find(pbeg, static_cast<size_t>(pend - pbeg), chDelim);
   });
}

//--------------------------------------------------------------------------//

/// \ingroup AlNum
/// 分割字串, 並移除分割字元 chDelim 左右的空白.
/// - 在沒找到 chDelim 時:
///   - 返回 src (沒有移除任何空白), src 會設為 nullptr.
/// - 在有找到 chDelim 時, 移除 chDelim 左右的空白:
///   - src 會移除前方空白, 尾端不動.
///   - 傳回值會移除尾端空白, 但前方不動.
fon9_API StrView StrSplit(StrView& src, char chDelim);

/// \ingroup AlNum
/// 移除 src 前後空白後, 然後使用 StrSplit() 分割字串.
inline StrView StrTrimSplit(StrView& src, char chDelim) {
   return StrSplit(StrTrim(&src), chDelim);
}

/// \ingroup AlNum
/// 簡單的從 src 取出一組 `tag=value`, src = `tag=value|tag=value|tag=value...`.
/// - 若 src 移除開頭空白後為 empty(), 則返回 false.
/// - tag, value 都會移除前後空白.
/// - 如果沒有找到 chEqual, 則 value 為 nullptr.
/// - src.begin() 移到 chFieldDelim 分割位置+1(沒有移除空白), 如果沒有 chFieldDelim, 則移到 src.end()
/// - **不考慮括號** 包住的巢狀欄位, 如果有需要考慮括號, 則應使用 `SbrFetchFieldNoTrim()`
fon9_API bool StrFetchTagValue(StrView& src, StrView& tag, StrView& value, char chFieldDelim = '|', char chEqual = '=');

/// \ingroup AlNum
/// 若第一個字元為引號「單引號 ' or 雙引號 " or 反引號 `」, 則移除第一個字元引號, 然後:
/// - 若最後字元為對應的引號, 則移除最後引號字元.
/// - if(pQuote)  *pQuote = 引號字元.
/// - src 內容不變.
inline StrView StrNoTrimRemoveQuotes(StrView src, char* pQuote = nullptr) {
   switch (int ch = src.Get1st()) {
   case '\'': case '\"': case '`':
      src.SetBegin(src.begin() + 1);
      if (!src.empty() && *(src.end() - 1) == ch)
         src.SetEnd(src.end() - 1);
      if (pQuote)
         *pQuote = static_cast<char>(ch);
      break;
   default:
      if (pQuote)
         *pQuote = 0;
      break;
   }
   return src;
}

/// \ingroup AlNum
/// 移除前後空白後, 如果有引號(單引號 or 雙引號 or 反引號), 則移除引號.
/// \ref StrNoTrimRemoveQuotes()
inline StrView StrTrimRemoveQuotes(StrView src, char* pQuote = nullptr) {
   return StrNoTrimRemoveQuotes(StrTrim(&src), pQuote);
}

//--------------------------------------------------------------------------//

/// \ingroup AlNum
/// 括號配對定義.
struct StrBrPair {
   /// 起始括號字元:「 '、"、{、[、( 」
   char     Left_;
   /// 結束括號字元:「 '、"、}、]、) 」
   char     Right_;
   /// 是否允許巢狀括號.
   bool     IsAllowNest_;
};

/// \ingroup AlNum
/// 字串解析時的括號設定參數.
struct fon9_API StrBrArg {
   /// 括號配對起始設定.
   const StrBrPair*  BrBegin_;
   /// 有幾個括號配對設定.
   size_t            BrCount_;

   /// 指定數量的「括號對」設定.
   constexpr StrBrArg(const StrBrPair* beg, size_t count) : BrBegin_(beg), BrCount_(count) {
   }

   /// 用「括號對」陣列建構.
   template <size_t arysz>
   constexpr StrBrArg(const StrBrPair(&brAry)[arysz]) : StrBrArg{&brAry[0],arysz} {
   }

   /// 在 [BrBegin_..end)->Left_ 尋找 chLeft.
   /// 若 chLeft 沒找到, 則傳回 nullptr;
   const StrBrPair* Find(char chLeft) const;

   /// 預設的括號設定參數: 「 {}、[]、()、''、"" 」
   static const StrBrArg   Default_;
   /// 排除大括號之後的括號設定參數: 「 []、()、''、"" 」
   static const StrBrArg   DefaultNoCurly_;
   /// 引號設定參數: 「 ''、"", `` 」
   static const StrBrArg   Quotation_;
};

/// \ingroup AlNum
/// 從 src 取出一個用 chDelim 分隔的字串.
/// - 用 brArg 考慮各種括號、引號, 例如: "{apple|orange|(car|bus)}|1234|5678"; 則傳回 "{apple|orange|(car|bus)}", src = "1234|5678"
/// - 若有 '\' 則 '\' 的下一個字元, 不考慮是否為括號.
/// - 若沒找到 chDelim 則傳回 src, 且將 src 移到尾端: src.SetBegin(src.end()).
/// - 若有找到 chDelim 則傳回 StrView{src.begin(), pDelim}; 且將 src 移到 pDelim 的下一個字元: src.SetBegin(pDelim+1);
/// - 傳回值 & src 都不會移除任何的空白.
/// \code
///   while (!cfgstr.empty()) {
///      fon9::StrView value = fon9::FetchField(cfgstr, '|');
///      fon9::StrView tag = fon9::StrTrimSplit(value, '=');
///      ...處理 tag & value...
///   }
/// \endcode
fon9_API StrView SbrFetchFieldNoTrim(StrView& src, char chDelim, const StrBrArg& brArg = StrBrArg::Default_);

/// \ingroup AlNum
/// 取出「括號或引號」內的字串.
/// - 如果 *src.begin() 有左括號(由brArg定義), 且有找到對應的右括號:
///   - retval = 括號內的字串(包含空白), *retval.end()==右括號.
///   - src = {右括號位置+1, src.end()}
/// - 如果 *src.begin() 有左括號, 但沒找到對應的右括號:
///   - retval = 移除開頭左括號後的字串
///   - src = {retval.end(), retval.end()}
/// - 如果 *src.begin() 沒左括號:
///   - retval = nullptr
///   - src = 不變.
fon9_API StrView SbrFetchInsideNoTrim(StrView& src, const StrBrArg& brArg = StrBrArg::Default_);
/// \ingroup AlNum
/// 先移除 src 開頭空白, 然後使用 FetchFirstBrNoTrim() 取出「括號或引號」內的字串.
inline StrView SbrTrimHeadFetchInside(StrView& src, const StrBrArg& brArg = StrBrArg::Default_) {
   if (!StrTrimHead(&src).empty())
      return SbrFetchInsideNoTrim(src, brArg);
   return StrView{nullptr};
}

//--------------------------------------------------------------------------//

/// \ingroup AlNum
/// 正規化成標準的 C 字串(http://en.cppreference.com/w/c/language/escape): 將「`\xxx`」轉成指定的字元.
fon9_API void StrView_ToNormalizeStr(std::string& dst, StrView src);
inline std::string StrView_ToNormalizeStr(StrView src) {
   std::string dst;
   StrView_ToNormalizeStr(dst, src);
   return dst;
}

/// \ingroup AlNum
/// 將字串 src 轉成使用 '\' 字元表達特殊字元的字串, 附加到 dst 的尾端.
/// - 例如: '\\n', '\\r', '\'', '\"',
/// - 若字元<='\\x1f' 則用 '\\xhh' 表示, hh=固定2碼小寫16進位數字.
/// - 頭尾不包含引號.
/// - 可自訂一組特殊字元 chSpecials, 當發現特殊字元, 會使用 '\' + ch 表示.
fon9_API void StrView_ToEscapeStr(std::string& dst, StrView src, StrView chSpecials = StrView{});
inline std::string StrView_ToEscapeStr(StrView src, StrView chSpecials = StrView{}) {
   std::string dst;
   StrView_ToEscapeStr(dst, src, chSpecials);
   return dst;
}

/// \ingroup AlNum
/// 如果 utf8str 長度超過 expectLen 則切除超過的部分,
/// 如果切除的位置剛好是一個 [utf8字] 的一部份, 則長度會再縮減, 避免有被切斷的 [utf8字].
/// \return 傳回切割後的字串, 不會變動 utf8str.
fon9_API StrView StrView_TruncUTF8(StrView utf8str, size_t expectLen);

//--------------------------------------------------------------------------//

/// \ingroup AlNum
/// 在 fullstr 裡面尋找 substr, 參數範例:
/// - fullstr = "aaa bbb ccc ddd"
/// - substr = "ccc"
/// - chSplitter = ' '
fon9_API const char* StrSearchSubstr(StrView fullstr, StrView substr, char chSplitter);

template <class StrT>
StrT StrReplaceImpl(StrView src, const StrView oldStr, const StrView newStr) {
   if (oldStr.empty())
      return StrT{};
   StrT res;
   const size_t oldsz = oldStr.size();
   if (src.size() < oldsz) {
      src.AppendTo(res);
      return res;
   }
   res.reserve(src.size());
   StrView dst{src.begin(), src.size() - oldsz + 1};
   while (const char* p1st = dst.Find(*oldStr.begin())) {
      if (memcmp(p1st + 1, oldStr.begin() + 1, oldsz - 1) == 0) {
         res.append(src.begin(), p1st);
         res.append(newStr.begin(), newStr.end());
         src.SetBegin(p1st += oldsz);
         if (p1st >= dst.end())
            break;
         dst.SetBegin(p1st);
      }
      else
         dst.SetBegin(p1st + 1);
   }
   res.append(src.begin(), src.end());
   return res;
}

fon9_API std::string StdStrReplace(StrView src, const StrView oldStr, const StrView newStr);
fon9_API CharVector CharVectorReplace(StrView src, const StrView oldStr, const StrView newStr);

} // namespace fon9
#endif//__fon9_StrTools_hpp__
