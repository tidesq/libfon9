/// \file fon9/seed/FieldChars.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_FieldChars_hpp__
#define __fon9_seed_FieldChars_hpp__
#include "fon9/seed/Field.hpp"
#include "fon9/seed/RawWr.hpp"
#include "fon9/CharAry.hpp"

namespace fon9 { namespace seed {

/// \ingroup seed
/// 存取字元陣列: char[] 的欄位.
class fon9_API FieldChars : public Field {
   fon9_NON_COPY_NON_MOVE(FieldChars);
   using base = Field;
protected:
   using base::base;

public:
   /// 建構:
   /// - 固定為 FieldType::Chars; FieldSource::DataMember;
   /// - size 必須大於0, 若有動態調整大小的需求, 應使用 FieldStdString 或 FieldCharVector.
   FieldChars(Named&& named, int32_t ofs, size_t size)
      : base(std::move(named), FieldType::Chars, FieldSource::DataMember, ofs, size, 0) {
      assert(size > 0);
   }
   /// DyMem 建構, 所以不用提供 ofs:
   /// - 固定為 FieldType::Chars; FieldSource::DyMem;
   FieldChars(Named&& named, size_t size)
      : base(std::move(named), FieldType::Chars, FieldSource::DyMem, 0, size, 0) {
      assert(size > 0);
   }

   StrView GetValue(const RawRd& rd) const {
      const char* ptr = rd.GetCellPtr<char>(*this);
      if (const char* pend = reinterpret_cast<const char*>(memchr(ptr, 0, this->Size_)))
         return StrView{ptr, pend};
      return StrView{ptr, this->Size_};
   }
   StrView GetStrView(const RawRd& rd) const {
      return this->GetValue(rd);
   }

   /// 傳回: "Cn"; n = this->Size_;
   virtual StrView GetTypeId(NumOutBuf&) const override;

   virtual void CellRevPrint(const RawRd& rd, StrView fmt, RevBuffer& out) const override;
   virtual OpResult StrToCell(const RawWr& wr, StrView value) const override;

   /// 第1個字元 = '\0' 其餘不變.
   virtual OpResult SetNull(const RawWr& wr) const override;
   /// \retval 第1個字元=='\0';
   virtual bool IsNull(const RawRd& rd) const override;

   virtual FieldNumberT GetNumber(const RawRd& rd, DecScaleT outDecScale, FieldNumberT nullValue) const override;
   virtual OpResult PutNumber(const RawWr& wr, FieldNumberT num, DecScaleT decScale) const override;

   virtual OpResult Copy(const RawWr& wr, const RawRd& rd) const override;
};

template <size_t arysz>
inline FieldSPT<FieldChars> MakeField(Named&& named, int32_t ofs, char(&)[arysz]) {
   return FieldSPT<FieldChars>{new FieldChars{std::move(named), ofs, arysz}};
}
template <size_t arysz>
inline FieldSPT<FieldChars> MakeField(Named&& named, int32_t ofs, const char(&)[arysz]) {
   return FieldSPT<FieldChars>{new FieldConst<FieldChars>{std::move(named), ofs, arysz}};
}
template <size_t arysz>
inline FieldSPT<FieldChars> MakeField(Named&& named, int32_t ofs, CharAry<arysz, char>&) {
   return FieldSPT<FieldChars>{new FieldChars{std::move(named), ofs, arysz}};
}
template <size_t arysz>
inline FieldSPT<FieldChars> MakeField(Named&& named, int32_t ofs, const CharAry<arysz, char>&) {
   return FieldSPT<FieldChars>{new FieldConst<FieldChars>{std::move(named), ofs, arysz}};
}

//--------------------------------------------------------------------------//

/// \ingroup seed
/// 存取 Raw 裡面的一個字元, 字元值 '\0' 表示 Null.
class fon9_API FieldChar1 : public FieldChars {
   fon9_NON_COPY_NON_MOVE(FieldChar1);
   using base = FieldChars;
public:
   /// DataMember = char: 大小固定為 1 byte.
   FieldChar1(Named&& named, int32_t ofs) : base(std::move(named), ofs, 1) {
   }
   /// DyMem = char: 大小固定為 1 byte.
   FieldChar1(Named&& named) : base(std::move(named), 1) {
   }

   char GetValue(const RawRd& rd) const {
      return *rd.GetCellPtr<char>(*this);
   }
   char GetChar1(const RawRd& rd) const {
      return this->GetValue(rd);
   }

   /// 傳回: "C1";
   virtual StrView GetTypeId(NumOutBuf&) const override;

   virtual void CellRevPrint(const RawRd& rd, StrView fmt, RevBuffer& out) const override;
   virtual OpResult StrToCell(const RawWr& wr, StrView value) const override;

   virtual OpResult Copy(const RawWr& wr, const RawRd& rd) const override;
};

inline FieldSPT<FieldChar1> MakeField(Named&& named, int32_t ofs, char&) {
   return FieldSPT<FieldChar1>{new FieldChar1{std::move(named), ofs}};
}
inline FieldSPT<FieldChar1> MakeField(Named&& named, int32_t ofs, const char&) {
   return FieldSPT<FieldChar1>{new FieldConst<FieldChar1>{std::move(named), ofs}};
}

} } // namespaces
#endif//__fon9_seed_FieldChars_hpp__
