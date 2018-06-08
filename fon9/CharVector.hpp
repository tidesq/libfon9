/// \file fon9/CharVector.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_CharVector_hpp__
#define __fon9_CharVector_hpp__
#include "fon9/ByteVector.hpp"

namespace fon9 {

/// \ingroup Misc
/// - 當 key 使用時, 可用來取代 std::string
/// - 在資料量 <= ByteVector::kMaxBinsSize 時, 不用分配記憶體.
struct CharVector : public ByteVector {
   using ByteVector::ByteVector;

   static const CharVector MakeRef(const StrView& str) {
      return CharVector{str.begin(), str.size()};
   }
   static const CharVector MakeRef(const std::string& str) {
      return CharVector{str.c_str(), str.size()};
   }
   static const CharVector MakeRef(const void* mem, size_t sz) {
      return CharVector{mem,sz};
   }
   static const CharVector MakeRef(const void* pbeg, const void* pend) {
      return CharVector{pbeg,static_cast<size_t>(reinterpret_cast<const char*>(pend) - reinterpret_cast<const char*>(pbeg))};
   }

   using ByteVector::append;
   void append(size_t sz, char ch) {
      ByteVector::append(sz, static_cast<byte>(ch));
   }
   void push_back(char ch) {
      this->append(1, ch);
   }

   inline friend StrView ToStrView(const CharVector& pthis) {
      if (pthis.Key_.BinsSize_ < 0)
         return StrView{};
      if (pthis.Key_.BinsSize_ > 0)
         return StrView{reinterpret_cast<const char*>(pthis.Key_.Bins_), static_cast<size_t>(pthis.Key_.BinsSize_)};
      return StrView{reinterpret_cast<const char*>(pthis.Key_.Blob_.MemPtr_), pthis.Key_.Blob_.MemUsed_};
   }

   const char* begin() const { return reinterpret_cast<const char*>(ByteVector::begin()); }
   const char* end() const { return reinterpret_cast<const char*>(ByteVector::end()); }
   char* begin() { return reinterpret_cast<char*>(ByteVector::begin()); }
   char* end() { return reinterpret_cast<char*>(ByteVector::end()); }

   void AppendTo(std::string& out) const {
      out.append(this->begin(), this->size());
   }
   std::string ToString() const {
      return std::string{this->begin(), this->size()};
   }
};
StrView ToStrView(const CharVector& pthis);

} // namespaces
#endif//__fon9_CharVector_hpp__
