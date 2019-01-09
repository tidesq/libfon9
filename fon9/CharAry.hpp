/// \file fon9/CharAry.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_CharAry_hpp__
#define __fon9_CharAry_hpp__
#include "fon9/StrView.hpp"

namespace fon9 {

/// \ingroup AlNum
/// 因為 `const char CaID_[n];` 無法在建構時設定內容,
/// 所以設計此型別 `const CharAry<n> CaID_;`
/// 在建構時可用底下方式設定 CaID_ 的內容:
///   `MyObj::MyObj() : CaID_("MyID") {}`
/// 當然也是可以在一般無 const 的情況下使用.
template <size_t arysz, typename CharT = char>
struct CharAry : public Comparable<CharAry<arysz, CharT>> {
   using value_type = CharT;

   /// 字元陣列.
   CharT  Chars_[arysz];

   /// 預設建構, Chars_ 全部為 **未定義** 的內容.
   CharAry() = default;
   /// 建構時可設定內容.
   CharAry(const StrView& str) {
      this->AssignFrom(str);
   }

   /// 從 StrView 設定內容.
   /// 若 str.size() < arysz, 則尾端填滿 '\0'
   void AssignFrom(StrView src) {
      size_t szSrc = src.size();
      if (szSrc < arysz) {
         memmove(this->Chars_, src.begin(), szSrc);
         memset(this->Chars_ + szSrc, 0, arysz - szSrc);
      }
      else
         memmove(this->Chars_, src.begin(), arysz);
   }
   void Clear(char ch = 0) {
      memset(this->Chars_, ch, arysz);
   }

   /// 使用 ToStrView() 來比較.
   int Compare(const CharAry& rhs) const {
      return ToStrView(*this).Compare(ToStrView(rhs));
   }
   bool operator<(const CharAry& rhs) const {
      return this->Compare(rhs) < 0;
   }
   bool operator==(const CharAry& rhs) const {
      return ToStrView(*this) == ToStrView(rhs);
   }

   const char* begin() const { return this->Chars_; }
   /// end() 為陣列尾端, 不考慮是否有 EOS.
   const char* end() const { return this->Chars_ + sizeof(this->Chars_); }
   char* begin() { return this->Chars_; }
   char* end() { return this->Chars_ + sizeof(this->Chars_); }
   
   char* data() { return this->Chars_; }
   const char* data() const { return this->Chars_; }

   constexpr size_t size() const { return arysz; }
   constexpr size_t max_size() const { return arysz; }
};

/// 用 StrView 來表達 CharAry<>.
/// 如果 pthis.Chars_[] 有 EOS, 則 retval.end() 指向第一個 EOS.
template <size_t arysz, typename CharT>
StrView ToStrView(const CharAry<arysz, CharT>& pthis) {
   return StrView_eos_or_all(pthis.Chars_);
}

} // namespace fon9
#endif//__fon9_CharAry_hpp__
