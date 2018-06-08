/// \file fon9/seed/RawRd.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_RawRd_hpp__
#define __fon9_seed_RawRd_hpp__
#include "fon9/seed/Raw.hpp"
#include "fon9/Exception.hpp"

namespace fon9 { namespace seed {

fon9_MSC_WARN_DISABLE(4623); // 4623: 'OpRawError': default constructor was implicitly defined as deleted
fon9_DEFINE_EXCEPTION(OpRawError, std::runtime_error);
fon9_MSC_WARN_POP;

/// \ingroup seed
/// RawRd, RawWr 的共同基底.
class RawRd {
   fon9_NON_COPY_NON_MOVE(RawRd);
protected:
   const byte*    RawBase_;
   const uint32_t DyMemPos_;
   const uint32_t DyMemSize_;

   RawRd(const byte* rawBase) : RawBase_{rawBase}, DyMemPos_{0}, DyMemSize_{0} {
   }
   RawRd(const Raw* raw)
      : RawBase_{reinterpret_cast<const byte*>(raw)}
      , DyMemPos_{raw->DyMemPos_}
      , DyMemSize_{raw->DyMemSize_} {
   }

public:
   /// 用 Field 取得資料內容.
   /// 只有在 DyMem 欄位, 才會用到 tmpbuf.
   template <typename RtnType>
   const RtnType& GetCellValue(const Field& fld, RtnType& tmpbuf) const {
      assert(fld.Size_ == sizeof(RtnType));
      if (fon9_LIKELY(fld.Source_ < FieldSource::DyMem))
         return *reinterpret_cast<const RtnType*>(this->RawBase_ + fld.Offset_);
      int32_t  ofs = fld.Offset_;
      if (fon9_LIKELY(this->DyMemSize_ >= ofs + sizeof(RtnType))) {
         ofs += this->DyMemPos_;
         tmpbuf = GetUnaligned(reinterpret_cast<const RtnType*>(this->RawBase_ + ofs));
         return tmpbuf;
      }
      /// DyMemSize 不足: 擁有動態欄位的 Raw 沒有使用 MakeDyMemRaw() 來建立?
      Raise<OpRawError>("Raw.GetCellValue: Bad raw.DyMemSize or fld.Offset");
   }

   /// 用 Field 取得資料內容的位置, 不保證記憶體對齊.
   template <typename RtnType>
   const RtnType* GetCellPtr(const Field& fld) const {
      assert(fld.Size_ >= sizeof(RtnType));
      if (fon9_LIKELY(fld.Source_ < FieldSource::DyMem))
         return reinterpret_cast<const RtnType*>(this->RawBase_ + fld.Offset_);
      int32_t  ofs = fld.Offset_;
      if (fon9_LIKELY(this->DyMemSize_ >= ofs + fld.Size_)) {
         ofs += this->DyMemPos_;
         return reinterpret_cast<const RtnType*>(this->RawBase_ + ofs);
      }
      /// DyMemSize 不足: 擁有動態欄位的 Raw 沒有使用 MakeDyMemRaw() 來建立?
      Raise<OpRawError>("Raw.GetCellPtr: Bad raw.DyMemSize or fld.Offset");
   }

   /// 用於只能是 data member 的欄位.
   /// e.g. FieldStdString 存取 std::string DataMember_;
   template <typename RtnType>
   const RtnType& GetMemberCell(const Field& fld) const {
      assert(fld.Size_ == sizeof(RtnType));
      assert(fld.Source_ == FieldSource::DataMember);
      return *reinterpret_cast<const RtnType*>(this->RawBase_ + fld.Offset_);
   }
};

struct SimpleRawRd : public RawRd {
   fon9_NON_COPY_NON_MOVE(SimpleRawRd);
public:
   SimpleRawRd(const byte* rawBase) : RawRd(rawBase) {
   }
   SimpleRawRd(const Raw* raw) : RawRd(raw) {
   }
};

} } // namespaces
#endif//__fon9_seed_RawRd_hpp__
