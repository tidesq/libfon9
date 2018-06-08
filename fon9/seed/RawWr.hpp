/// \file fon9/seed/RawWr.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_RawWr_hpp__
#define __fon9_seed_RawWr_hpp__
#include "fon9/seed/RawRd.hpp"

namespace fon9 { namespace seed {

/// \ingroup seed
/// 要異動 Raw 時, 必須取得此物件才能更新.
/// - 何時真正結束異動, 由提供者決定, 可能的情況:
///   - RawWr死亡時立即觸發結束更新.
///   - 在所有指向相同 Raw 的 RawWr 物件結束更新(or死亡), 才真正的結束異動.
///   - ...其他...
/// - 取得後必須立即使用, 並盡快結束更新, 保留RawWr指標 或 複製RawWr, 都是不正確的用法!
class RawWr : public RawRd {
   fon9_NON_COPY_NON_MOVE(RawWr);
   using base = RawRd;
protected:

   RawWr(byte* rawBase) : base(rawBase) {
   }
   RawWr(Raw* raw) : base(raw) {
   }

public:
   template <typename ValueType>
   void PutCellValue(const Field& fld, const ValueType& value) const {
      assert(fld.Size_ == sizeof(ValueType));
      if (fon9_LIKELY(fld.Source_ < FieldSource::DyMem))
         *reinterpret_cast<ValueType*>(const_cast<byte*>(this->RawBase_) + fld.Offset_) = value;
      else {
         int32_t  ofs = fld.Offset_;
         if (fon9_LIKELY(this->DyMemSize_ >= ofs + sizeof(ValueType))) {
            ofs += this->DyMemPos_;
            PutUnaligned(reinterpret_cast<ValueType*>(const_cast<byte*>(this->RawBase_) + ofs), value);
         }
         else /// DyMemSize 不足: 擁有動態欄位的 Raw 沒有使用 MakeDyMemRaw() 來建立?
            Raise<OpRawError>("Raw.PutCellValue: Bad raw.DyMemSize or fld.Offset");
      }
   }

   /// 用 Field 取得資料內容的位置, 不保證記憶體對齊.
   template <typename RtnType>
   RtnType* GetCellPtr(const Field& fld) const {
      return const_cast<RtnType*>(base::GetCellPtr<RtnType>(fld));
   }

   /// 用於只能是 data member 的欄位.
   /// e.g. FieldStdString 存取 std::string DataMember_;
   template <typename RtnType>
   RtnType& GetMemberCell(const Field& fld) const {
      assert(sizeof(RtnType) == fld.Size_);
      assert(fon9_LIKELY(fld.Source_ == FieldSource::DataMember));
      return *reinterpret_cast<RtnType*>(const_cast<byte*>(this->RawBase_) + fld.Offset_);
   }
};

struct SimpleRawWr : public RawWr {
   fon9_NON_COPY_NON_MOVE(SimpleRawWr);
public:
   SimpleRawWr(byte* rawBase) : RawWr(rawBase) {
   }
   SimpleRawWr(Raw* raw) : RawWr(raw) {
   }
};

} } // namespaces
#endif//__fon9_seed_RawWr_hpp__
