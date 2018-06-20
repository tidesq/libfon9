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

struct UserDefineRaw {
   virtual const void* GetCellValue(const Field& fld, void* tmpbuf) = 0;
   virtual void PutCellValue(const Field& fld, const void* value) = 0;
   virtual const void* GetCellMemberPtrRd(const Field& fld) = 0;
   virtual void* GetCellMemberPtrWr(const Field& fld) = 0;
};

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

   template <typename RtnType>
   const RtnType* GetCellPtrImpl(const Field& fld) const {
      assert(fld.Size_ >= sizeof(RtnType));
      if (fon9_LIKELY(fld.Source_ == FieldSource::DataMember))
         return reinterpret_cast<const RtnType*>(this->RawBase_ + fld.Offset_);
      if (fon9_LIKELY(fld.Source_ >= FieldSource::DyMem)) {
         int32_t  ofs = fld.Offset_;
         if (fon9_LIKELY(this->DyMemSize_ >= ofs + fld.Size_)) {
            ofs += this->DyMemPos_;
            return reinterpret_cast<const RtnType*>(this->RawBase_ + ofs);
         }
         /// DyMemSize 不足: 擁有動態欄位的 Raw 沒有使用 MakeDyMemRaw() 來建立?
         Raise<OpRawError>("Raw.GetCellPtr: Bad raw.DyMemSize or fld.Offset");
      }
      assert(fld.Source_ == FieldSource::UserDefine);
      return nullptr;
   }

public:
   UserDefineRaw* UserDefineRaw_{nullptr};

   /// 用 Field 取得資料內容.
   /// 只有在 DyMem 欄位, 才會用到 tmpbuf.
   template <typename RtnType>
   const RtnType& GetCellValue(const Field& fld, RtnType& tmpbuf) const {
      assert(fld.Size_ == sizeof(RtnType));
      if (fon9_LIKELY(fld.Source_ == FieldSource::DataMember))
         return *reinterpret_cast<const RtnType*>(this->RawBase_ + fld.Offset_);
      if (fon9_LIKELY(fld.Source_ >= FieldSource::DyMem)) {
         int32_t  ofs = fld.Offset_;
         if (fon9_LIKELY(this->DyMemSize_ >= ofs + sizeof(RtnType))) {
            ofs += this->DyMemPos_;
            tmpbuf = GetUnaligned(reinterpret_cast<const RtnType*>(this->RawBase_ + ofs));
            return tmpbuf;
         }
         /// DyMemSize 不足: 擁有動態欄位的 Raw 沒有使用 MakeDyMemRaw() 來建立?
         Raise<OpRawError>("Raw.GetCellValue: Bad raw.DyMemSize or fld.Offset");
      }
      assert(fld.Source_ == FieldSource::UserDefine);
      return *reinterpret_cast<const RtnType*>(this->UserDefineRaw_->GetCellValue(fld, &tmpbuf));
   }

   /// 用 Field 取得資料內容的位置, 不保證記憶體對齊.
   template <typename RtnType>
   const RtnType* GetCellPtr(const Field& fld) const {
      if (const RtnType* retval = this->GetCellPtrImpl<RtnType>(fld))
         return retval;
      assert(fld.Source_ == FieldSource::UserDefine);
      return reinterpret_cast<const RtnType*>(this->UserDefineRaw_->GetCellMemberPtrRd(fld));
   }

   /// 用於只能是 data member 的欄位.
   /// e.g. FieldStdString 存取 std::string DataMember_;
   template <typename RtnType>
   const RtnType& GetMemberCell(const Field& fld) const {
      assert(fld.Size_ == sizeof(RtnType));
      if (fon9_LIKELY(fld.Source_ == FieldSource::DataMember))
         return *reinterpret_cast<const RtnType*>(this->RawBase_ + fld.Offset_);
      assert(fld.Source_ == FieldSource::UserDefine);
      return *reinterpret_cast<const RtnType*>(this->UserDefineRaw_->GetCellMemberPtrRd(fld));
   }
};

template <class DerivedRaw>
constexpr auto CastToRawPointer(DerivedRaw* d) -> enable_if_t<std::is_base_of<Raw, DerivedRaw>::value, Raw*> {
   return d;
}

template <class DerivedRaw>
constexpr auto CastToRawPointer(const DerivedRaw* d) -> enable_if_t<std::is_base_of<Raw, DerivedRaw>::value, const Raw*> {
   return d;
}

template <class DerivedRaw>
constexpr auto CastToRawPointer(DerivedRaw* d) -> enable_if_t<!std::is_base_of<Raw, DerivedRaw>::value, byte*> {
   return static_cast<byte*>(static_cast<void*>(d));
}

template <class DerivedRaw>
constexpr auto CastToRawPointer(const DerivedRaw* d) -> enable_if_t<!std::is_base_of<Raw, DerivedRaw>::value, const byte*> {
   return static_cast<const byte*>(static_cast<const void*>(d));
}

class SimpleRawRd : public RawRd {
   fon9_NON_COPY_NON_MOVE(SimpleRawRd);
public:
   SimpleRawRd(const byte* rawBase) : RawRd(rawBase) {
   }
   SimpleRawRd(const Raw* raw) : RawRd(raw) {
   }
   
   template <class Seed, typename SFINAE = enable_if_t<!std::is_pointer<Seed>::value, void>>
   SimpleRawRd(Seed& seed, SFINAE* = nullptr) : RawRd(CastToRawPointer(&seed)) {
   }
};

} } // namespaces
#endif//__fon9_seed_RawRd_hpp__
