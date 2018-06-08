/// \file fon9/seed/FieldChars.cpp
/// \author fonwinz@gmail.com
#include "fon9/seed/FieldChars.hpp"
#include "fon9/StrTo.hpp"
#include "fon9/RevPrint.hpp"

namespace fon9 { namespace seed {

StrView FieldChars::GetTypeId(NumOutBuf& nbuf) const {
   nbuf.SetEOS();
   char* pbeg = UIntToStrRev(nbuf.end(), this->Size_);
   *--pbeg = 'C';
   return StrView{pbeg, nbuf.end()};
}

void FieldChars::CellRevPrint(const RawRd& rd, StrView fmt, RevBuffer& out) const {
   FmtRevPrint(fmt, out, this->GetValue(rd));
}

OpResult FieldChars::StrToCell(const RawWr& wr, StrView value) const {
   char* ptr = wr.GetCellPtr<char>(*this);
   size_t sz = std::min(value.size(), static_cast<size_t>(this->Size_));
   memcpy(ptr, value.begin(), sz);
   if (sz < this->Size_)
      ptr[sz] = '\0';
   return OpResult::no_error;
}

OpResult FieldChars::SetNull(const RawWr& wr) const {
   assert(this->Size_ > 0);
   *wr.GetCellPtr<char>(*this) = '\0';
   return OpResult::no_error;
}

bool FieldChars::IsNull(const RawRd& rd) const {
   return *rd.GetCellPtr<char>(*this) == '\0';
}

FieldNumberT FieldChars::GetNumber(const RawRd& rd, DecScaleT outDecScale, FieldNumberT nullValue) const {
   return StrToDec(this->GetValue(rd), outDecScale, nullValue);
}

OpResult FieldChars::PutNumber(const RawWr& wr, FieldNumberT num, DecScaleT decScale) const {
   NumOutBuf      nbuf;
   char*          pbeg = DecToStrRev(nbuf.end(), num, decScale);
   const size_t   sz = nbuf.GetLength(pbeg);
   if (sz > this->Size_)
      return OpResult::value_overflow;
   char* ptr = wr.GetCellPtr<char>(*this);
   memcpy(ptr, pbeg, sz);
   if (sz < this->Size_)
      ptr[sz] = 0;
   return OpResult::no_error;
}

OpResult FieldChars::Copy(const RawWr& wr, const RawRd& rd) const {
   memcpy(wr.GetCellPtr<char>(*this), rd.GetCellPtr<char>(*this), this->Size_);
   return OpResult::no_error;
}

//--------------------------------------------------------------------------//

StrView FieldChar1::GetTypeId(NumOutBuf&) const {
   return StrView{"C1"};
}

void FieldChar1::CellRevPrint(const RawRd& rd, StrView fmt, RevBuffer& out) const {
   FmtRevPrint(fmt, out, this->GetValue(rd));
}

OpResult FieldChar1::StrToCell(const RawWr& wr, StrView value) const {
   *wr.GetCellPtr<char>(*this) = value.empty() ? '\0' : *value.begin();
   return OpResult::no_error;
}

OpResult FieldChar1::Copy(const RawWr& wr, const RawRd& rd) const {
   *wr.GetCellPtr<char>(*this) = *rd.GetCellPtr<char>(*this);
   return OpResult::no_error;
}

} } // namespaces
