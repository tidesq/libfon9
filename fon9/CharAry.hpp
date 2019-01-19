/// \file fon9/CharAry.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_CharAry_hpp__
#define __fon9_CharAry_hpp__
#include "fon9/StrView.hpp"
#include <string.h>

namespace fon9 {

/// \ingroup AlNum
/// 因為 `const char CaID_[n];` 無法在建構時設定內容,
/// 所以設計此型別 `const CharAry<n> CaID_;`
/// 在建構時可用底下方式設定 CaID_ 的內容:
///   `MyObj::MyObj() : CaID_("MyID") {}`
/// 當然也是可以在一般無 const 的情況下使用.
template <size_t arysz, typename CharT = char, CharT kChFiller = 0>
struct CharAry : public Comparable<CharAry<arysz, CharT, kChFiller>> {
   using value_type = CharT;

   /// 字元陣列.
   CharT  Chars_[arysz];

   /// 預設建構, Chars_ 全部為 **未定義** 的內容.
   CharAry() = default;
   /// 建構時可設定內容.
   CharAry(const StrView& str) {
      this->CopyFrom(str.begin(), str.size());
   }

   template <size_t srcsz>
   CharAry(const CharT(&src)[srcsz]) {
      this->CopyFrom(src, srcsz - (src[srcsz - 1] == 0));
   }
   template <size_t srcsz>
   CharAry(CharT(&src)[srcsz]) = delete;

   void CopyFrom(const char* src, size_t srcsz) {
      if (srcsz < arysz) {
         memcpy(this->Chars_, src, srcsz);
         memset(this->Chars_ + srcsz, kChFiller, arysz - srcsz);
      }
      else
         memcpy(this->Chars_, src, arysz);
   }

   /// 從 StrView 設定內容.
   /// 若 str.size() < arysz, 則尾端填滿 filler()
   void MoveFrom(const char* src, size_t srcsz) {
      if (srcsz < arysz) {
         memmove(this->Chars_, src, srcsz);
         memset(this->Chars_ + srcsz, kChFiller, arysz - srcsz);
      }
      else
         memmove(this->Chars_, src, arysz);
   }

   /// 從 StrView 設定內容, src 可能是 this->Chars_ 的子集(this->Chars_ 的子字串).
   /// 若 str.size() < arysz, 則尾端填滿 filler()
   void AssignFrom(const StrView& src) {
      this->MoveFrom(src.begin(), src.size());
   }
   template <size_t srcsz>
   void AssignFrom(const CharT(&src)[srcsz]) {
      this->CopyFrom(src, srcsz - (src[srcsz - 1] == 0));
   }
   template <size_t srcsz>
   void AssignFrom(CharT(&src)[srcsz]) = delete;

   void Clear(CharT ch = kChFiller) {
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

   const CharT* begin() const { return this->Chars_; }
   /// end() 為陣列尾端, 不考慮是否有 EOS.
   const CharT* end() const { return this->Chars_ + sizeof(this->Chars_); }
   CharT* begin() { return this->Chars_; }
   CharT* end() { return this->Chars_ + sizeof(this->Chars_); }
   
   CharT* data() { return this->Chars_; }
   const CharT* data() const { return this->Chars_; }

   constexpr size_t size() const { return arysz; }
   constexpr size_t max_size() const { return arysz; }
   constexpr CharT filler() const { return kChFiller; }
};

/// \ingroup AlNum
/// 用 StrView 來表達 CharAry<>.
/// 如果 pthis.Chars_[] 有 EOS, 則 retval.end() 指向第一個 EOS.
template <size_t arysz, typename CharT>
StrView ToStrView(const CharAry<arysz, CharT>& pthis) {
   return StrView_eos_or_all(pthis.Chars_);
}

/// \ingroup AlNum
/// 使用 CharAry<> 但 ToStrView() 時, 固定使用全部的內容.
template <size_t arysz, typename CharT = char, CharT kChFiller = 0>
class CharAryF : public CharAry<arysz, CharT, kChFiller> {
   using base = CharAry<arysz, CharT, kChFiller>;
public:
   using base::base;
   CharAryF() = default;

   friend constexpr fon9::StrView ToStrView(const CharAryF& value) {
      return fon9::StrView{value.Chars_, value.size()};
   }
   template <class RevBuffer>
   friend inline void RevPrint(RevBuffer& rbuf, const CharAryF& value) {
      RevPutMem(rbuf, value.Chars_, value.size());
   }
};

} // namespace fon9
#endif//__fon9_CharAry_hpp__
