/// \file fon9/seed/FieldValueFmt.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_FieldValueFmt_hpp__
#define __fon9_seed_FieldValueFmt_hpp__
#include "fon9/seed/Field.hpp"
#include "fon9/seed/RawWr.hpp"
#include "fon9/StrTo.hpp"
#include "fon9/RevPrint.hpp"

namespace fon9 { namespace seed {

/// \ingroup seed
/// ValueT 必須提供:
/// - ValueT(FieldNumberT num, DecScaleT decScale);
/// - static constexpr ValueT ValueT::Null();
/// - constexpr OrigType ValueT::GetOrigValue() const;
template <class ValueT, class FmtT = AutoFmt<ValueT>>
class FieldValueFmt : public Field {
   fon9_NON_COPY_NON_MOVE(FieldValueFmt);
   using base = Field;
public:
   /// 建構:
   /// - 固定為 FieldSource::DataMember;
   FieldValueFmt(Named&& named, FieldType type, int32_t ofs, DecScaleT decScale)
      : base{std::move(named), type, FieldSource::DataMember, ofs, sizeof(ValueT), decScale} {
   }
   /// 建構:
   /// - 固定為 FieldSource::DyMem;
   FieldValueFmt(Named&& named, FieldType type, DecScaleT decScale)
      : base(std::move(named), type, FieldSource::DyMem, 0, sizeof(ValueT), decScale) {
   }

   ValueT GetValue(const RawRd& rd) const {
      ValueT tmpValue;
      return rd.GetCellValue(*this, tmpValue);
   }
   void PutValue(const RawWr& wr, ValueT value) const {
      wr.PutCellValue(*this, value);
   }

   virtual void CellRevPrint(const RawRd& rd, StrView fmt, RevBuffer& out) const override {
      ValueT value = this->GetValue(rd);
      if (fon9_LIKELY(value != ValueT::Null()))
         FmtRevPrint(fmt, out, value);
   }
   virtual OpResult StrToCell(const RawWr& wr, StrView value) const override {
      this->PutValue(wr, StrTo(value, ValueT::Null()));
      return OpResult::no_error;
   }

   virtual FieldNumberT GetNumber(const RawRd& rd, DecScaleT outDecScale, FieldNumberT nullValue) const override {
      ValueT value = this->GetValue(rd);
      if (fon9_LIKELY(value != ValueT::Null()))
         return static_cast<FieldNumberT>(AdjustDecScale(value.GetOrigValue(), this->DecScale_, outDecScale));
      return nullValue;
   }
   virtual OpResult PutNumber(const RawWr& wr, FieldNumberT num, DecScaleT decScale) const override {
      this->PutValue(wr, ValueT{num, decScale});
      return OpResult::no_error;
   }

   virtual OpResult SetNull(const RawWr& wr) const override {
      this->PutValue(wr, ValueT::Null());
      return OpResult::no_error;
   }
   virtual bool IsNull(const RawRd& rd) const override {
      return this->GetValue(rd) == ValueT::Null();
   }

   virtual OpResult Copy(const RawWr& wr, const RawRd& rd) const override {
      this->PutValue(wr, this->GetValue(rd));
      return OpResult::no_error;
   }
   virtual int Compare(const RawRd& lhs, const RawRd& rhs) const override {
      return this->GetValue(lhs).Compare(this->GetValue(rhs));
   }
};

} } // namespaces
#endif//__fon9_seed_FieldValueFmt_hpp__
