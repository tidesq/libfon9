/// \file fon9/seed/FieldDyBlob.cpp
/// \author fonwinz@gmail.com
#include "fon9/seed/FieldDyBlob.hpp"
#include "fon9/RevPrint.hpp"
#include "fon9/Base64.hpp"

namespace fon9 { namespace seed {

StrView FieldDyBlob::GetTypeId(NumOutBuf&) const {
   return this->Type_ == FieldType::Chars ? StrView{"C0"} : StrView{"B0"};
}

void FieldDyBlob::CellRevPrint(const RawRd& rd, StrView fmt, RevBuffer& out) const {
   const fon9_Blob blob = GetUnaligned(rd.GetCellPtr<fon9_Blob>(*this));
   if (blob.MemUsed_ == 0) {
      if (!fmt.empty())
         RevPrint(out, StrView{}, FmtDef{fmt});
      return;
   }
   if (this->Type_ == FieldType::Chars)
      FmtRevPrint(fmt, out, StrView{reinterpret_cast<char*>(blob.MemPtr_), blob.MemUsed_});
   else
      RevPutB64(out, blob.MemPtr_, blob.MemUsed_);
}

OpResult FieldDyBlob::StrToCell(const RawWr& wr, StrView value) const {
   fon9_Blob* ptr = wr.GetCellPtr<fon9_Blob>(*this);
   fon9_Blob blob = GetUnaligned(ptr);
   if (this->Type_ == FieldType::Chars)
      fon9_Blob_Set(&blob, value.begin(), static_cast<fon9_Blob_Size_t>(value.size()));
   else if (size_t bmax = Base64DecodeLength(value.size())) {
      if (fon9_UNLIKELY(bmax != static_cast<fon9_Blob_Size_t>(bmax)
                        || !fon9_Blob_Set(&blob, nullptr, static_cast<fon9_Blob_Size_t>(bmax)))) {
         fon9_Blob_Free(&blob);
         PutUnaligned(ptr, blob);
         return OpResult::mem_alloc_error;
      }
      auto r = Base64Decode(blob.MemPtr_, bmax, value.begin(), value.size());
      blob.MemUsed_ = static_cast<fon9_Blob_Size_t>(r.GetResult());
   }
   else
      blob.MemUsed_ = 0;
   PutUnaligned(ptr, blob);
   return OpResult::no_error;
}

FieldNumberT FieldDyBlob::GetNumber(const RawRd&, DecScaleT, FieldNumberT nullValue) const {
   return nullValue;
}
OpResult FieldDyBlob::PutNumber(const RawWr& wr, FieldNumberT num, DecScaleT decScale) const {
   (void)wr; (void)num; (void)decScale;
   return OpResult::not_supported_number;
}

OpResult FieldDyBlob::SetNull(const RawWr& wr) const {
   fon9_Blob* blob = wr.GetCellPtr<fon9_Blob>(*this);
   PutUnaligned(&blob->MemUsed_, static_cast<fon9_Blob_Size_t>(0));
   return OpResult::no_error;
}
bool FieldDyBlob::IsNull(const RawRd& rd) const {
   const fon9_Blob* blob = rd.GetCellPtr<fon9_Blob>(*this);
   return GetUnaligned(&blob->MemUsed_) == 0;
}

OpResult FieldDyBlob::Copy(const RawWr& wr, const RawRd& rd) const {
   fon9_Blob dst = GetUnaligned(wr.GetCellPtr<fon9_Blob>(*this));
   fon9_Blob src = GetUnaligned(rd.GetCellPtr<fon9_Blob>(*this));
   if (src.MemUsed_ > 0)
      fon9_Blob_Set(&dst, src.MemPtr_, src.MemUsed_);
   else
      dst.MemUsed_ = 0;
   wr.PutCellValue(*this, dst);
   return OpResult::no_error;
}
int FieldDyBlob::Compare(const RawRd& lhs, const RawRd& rhs) const {
   fon9_Blob L = GetUnaligned(lhs.GetCellPtr<fon9_Blob>(*this));
   fon9_Blob R = GetUnaligned(rhs.GetCellPtr<fon9_Blob>(*this));
   return fon9_CompareBytes(L.MemPtr_, L.MemUsed_, R.MemPtr_, R.MemUsed_);
}

} } // namespaces
