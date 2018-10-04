/// \file fon9/StrVref.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_StrVref_hpp__
#define __fon9_StrVref_hpp__
#include "fon9/StrView.hpp"

namespace fon9 {

/// \ingroup AlNum
/// 使用 uint32_t Pos_, Size_; 記錄子字串的位置.
struct StrVref {
   uint32_t Pos_{0};
   uint32_t Size_{0};

   StrView ToStrView(const char* strbegin) const {
      return StrView{strbegin + this->Pos_, this->Size_};
   }
   void FromStrView(const char* strbegin, StrView s) {
      this->SetPosSize(s.begin() - strbegin, s.size());
   }
   uint32_t End() const {
      return this->Pos_ + this->Size_;
   }
   void SetPosSize(size_t pos, size_t size) {
      this->SetPos(pos);
      this->SetSize(size);
   }
   void SetPosSize(ptrdiff_t pos, size_t size) {
      this->SetPos(pos);
      this->SetSize(size);
   }
   void SetPos(size_t pos) {
      this->Pos_ = static_cast<uint32_t>(pos);
      assert(this->Pos_ == pos); // overflow?
   }
   void SetPos(ptrdiff_t pos) {
      assert(pos >= 0);
      this->Pos_ = static_cast<uint32_t>(pos);
      assert(this->Pos_ == pos); // overflow?
   }
   void SetSize(size_t size) {
      this->Size_ = static_cast<uint32_t>(size);
      assert(this->Size_ == size); // overflow?
   }
};

} // namespace std
#endif//__fon9_StrVref_hpp__
