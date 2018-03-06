﻿/// \file fon9/StrView.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_StrView_hpp__
#define __fon9_StrView_hpp__
#include "fon9/sys/Config.hpp"
#include "fon9/Comparable.hpp"

#include <string>

namespace fon9 {

/// \ingroup AlNum
/// 類似 C++17 的 std::string_view.
/// - 僅考慮 UTF-8 字串.
/// - 參數傳遞建議直接使用傳值方式: `void foo(StrView arg);`
///   除非只是單純轉發, 則使用 `inline void foo(const StrView& arg) { bar(arg); }`
class StrView : public Comparable<StrView> {

   /// 禁止使用 char strbuf[128]; 來建構 StrView.
   /// - 應該在 strbuf 填妥內容後, 使用 `StrView_cstr(strbuf);` 或 `StrView_eos_or_all(strbuf);` 或 `StrView_all(strbuf);`
   /// - 此建構是為了讓 compiler 在使用 `char strbuf[128];` 建構時會產生錯誤:
   ///   \code
   ///   char    strbuf[128];
   ///   StrView sv1{strbuf};// VC: error C2248: 'fon9::StrView::StrView': cannot access private member declared in class 'fon9::StrView'
   ///   \endcode
   template <size_t arysz>
   StrView(char(&str)[arysz]) = delete;

   /// 讓 compiler 在使用 StrView(nullptr) 時會產生錯誤.
   /// \code
   /// StrView sv1{nullptr};//VC: error C2440: 'initializing': cannot convert from 'initializer list' to 'fon9::StrView'
   /// StrView sv2(nullptr);//VC: error C2668: 'fon9::StrView::StrView': ambiguous call to overloaded function
   /// \endcode
   StrView(const void* r) = delete;

   const char* Begin_;
   const char* End_;
public:
   using traits_type = std::char_traits<char>;

   constexpr StrView() : StrView{"", static_cast<size_t>(0u)} {
   }
   /// \param ibegin 字串開頭, 不可使用 nullptr!
   /// \param sz     字串長度.
   constexpr StrView(const char* ibegin, size_t sz) : Begin_{ibegin}, End_{ibegin + sz} {
   }

   /// 使用 [ibegin..iend) 方式建構.
   /// \param ibegin 字串開頭, 不可使用 nullptr!
   /// \param iend   字串結尾, 不可使用 nullptr!
   StrView(const char* ibegin, const char* iend) : Begin_{ibegin}, End_{iend} {
      assert(iend != nullptr);
      assert(ibegin <= iend);
   }

   /// 使用 `const char cstr[] = "abc";` 的字串建構 `StrView{cstr};` 也可直接使用 `StrView{"abc"}`.
   /// 不可使用 **非 const** 的字元陣列(char cstr[];).
   /// 用此方式建構, **不包含** 尾端的 EOS.
   /// \code
   ///   const char chary3NoEOS[]  = { 'a', 'b', 'c'  };
   ///   const char chary2HasEOS[] = { 'a', 'b', '\0' };
   ///   StrView s3{chary3NoEOS}; //長度=3.
   ///   StrView s2{chary2HasEOS};//長度=2.
   ///   StrView s5{"hello"};     //長度=5.
   ///
   ///   char strbuf[128];
   ///   // StrView sERR{strbuf}; strbuf 非 const, 建構失敗, MSVC: error C2248: 'fon9::StrView::StrView' : 無法存取 private 成員
   /// \endcode
   template <size_t arysz>
   constexpr StrView(const char(&str)[arysz]) : StrView{str, arysz - (str[arysz - 1] == 0)} {
      static_assert(arysz > 0, "cannot use 'const char ary[0];'");
   }

   /// 使用 std::string 建構.
   /// 在 `foo(&stdstr);` 呼叫返回前 stdstr 的值不能被改變(multi thread 須注意)!
   /// \param stdstr 不可使用 nullptr
   StrView(const std::string* stdstr) : StrView{stdstr->c_str(), stdstr->size()} {
   }

   /// 設定開始位置: ibegin 必定 <= this->end();
   /// 您必須自行確定 ibegin 是有效的.
   void SetBegin(const char* ibegin) {
      assert(ibegin <= this->end());
      this->Begin_ = ibegin;
   }

   /// 設定結束位置: iend 必定 >= this->begin();
   /// 您必須自行確定 iend 是有效的.
   void SetEnd(const char* iend) {
      assert(this->begin() <= iend);
      this->End_ = iend;
   }

   /// 重設 [開始、結束) 位置: ibegin 必定 <= iend;
   void Reset(const char* ibegin, const char* iend) {
      assert(ibegin <= iend);
      this->Begin_  = ibegin;
      this->End_ = iend;
   }
   

   /// 可與 std 配合使用的 iterator 及相關 methods.
   using const_iterator = const char*;
   /// 與 const_iterator 完全相同, StrView 不能改變內容, 所以 iterator 也不能改變內容.
   using iterator = const_iterator;

   /// 字元開始位置.
   /// 即使是 empty()==true 也會傳回有效值, 永遠不會傳回 nullptr,
   /// 但是直接使用 *begin() 仍然是危險的, 因為有可能 begin()==end() 但 *end() 不允許存取.
   /// 所以, 如果只是為了取得第1個字元, 可以考慮使用 Get1st();
   constexpr const char* begin() const {
      return this->Begin_;
   }
   constexpr const char* cbegin() const {
      return this->begin();
   }
   /// 字元結束位置.
   constexpr const char* end() const {
      return this->End_;
   }
   constexpr const char* cend() const {
      return this->end();
   }
   /// 取得字串是否為空(長度為0)?
   constexpr bool empty() const {
      return this->begin() == this->end();
   }
   /// 不含 EOS 的字元數量.
   constexpr size_t size() const {
      return static_cast<size_t>(end() - begin());
   }

   /// 取得第一個字元.
   /// \retval EOF      empty()==true.
   /// \retval *begin() static_cast<unsigned char>(*begin());
   constexpr int Get1st() const {
      return this->empty() ? EOF : static_cast<unsigned char>(*this->begin());
   }

   /// 建立 std::string
   std::string ToString() const {
      return std::string(begin(), end());
   }

   /// 比較大小, 若內容相同就相同, 不考慮是否有EOS.
   ///   - "abc" == { 'a', 'b', 'c' }
   ///   - StrView{} == StrView{""}
   ///
   /// \retval <0   *this < r;
   /// \retval ==0  *this == r;
   /// \retval >0   *this > r;
   int Compare(const StrView& r) const {
      const size_t lhsSize = this->size();
      const size_t rhsSize = r.size();
      const size_t minSize = (lhsSize < rhsSize ? lhsSize : rhsSize);
      if (const int retval = traits_type::compare(this->begin(), r.begin(), minSize))
         return retval;
      return (lhsSize < rhsSize ? -1
              : lhsSize > rhsSize ? 1
              : 0);
   }
   /// 判斷內容是否相等.
   ///   - "abc" == { 'a', 'b', 'c' }
   ///   - StrView{} == StrView{""}
   inline friend bool operator==(const StrView& lhs, const StrView& rhs) {
      const size_t sz1 = lhs.size();
      if (sz1 == rhs.size())
         return traits_type::compare(lhs.begin(), rhs.begin(), sz1) == 0;
      return false;
   }
   inline friend bool operator<(const StrView& lhs, const StrView& rhs) {
      return lhs.Compare(rhs) < 0;
   }
};

/// \ingroup AlNum
/// 使用一般 C 字串, 包含EOS, 長度需要使用strlen()取得的字串建構, cstr 可以是 nullptr.
constexpr StrView StrView_cstr(const char* cstr) {
   return cstr ? StrView{cstr, StrView::traits_type::length(cstr)} : StrView{};
}
inline StrView ToStrView(const char* cstr) {
   return StrView_cstr(cstr);
}
inline StrView ToStrView(const std::string& str) {
   return StrView{&str};
}

/// \ingroup AlNum
/// char cstr[arysz]; 若有包含EOS則到EOS, 否則包含全部.
template <size_t arysz>
inline StrView StrView_eos_or_all(const char(&cstr)[arysz]) {
   if (const char* peos = StrView::traits_type::find(cstr, arysz, '\0'))
      return StrView{cstr, peos};
   return StrView{cstr, cstr + arysz};
}
/// \ingroup AlNum
/// 使用整個 `const char[]` or `char[]` 建構, 不考慮尾端是否有EOS或空白.
template <size_t arysz>
constexpr StrView StrView_all(const char(&cary)[arysz]) {
   return StrView{cary, cary + arysz};
}
} // namespace fon9

namespace std {
template<>
struct hash<fon9::StrView> {
   size_t operator()(const fon9::StrView& val) const {
      // BKDR hash.
      const char* const end = val.end();
      const char* beg = val.begin();
      size_t hashed = 0;
      while (beg != end)
         hashed = hashed * 131 + *beg++;
      return hashed;
   }
};
} // namespace std
#endif//__fon9_StrView_hpp__