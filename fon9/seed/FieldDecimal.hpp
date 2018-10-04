/// \file fon9/seed/FieldDecimal.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_FieldDecimal_hpp__
#define __fon9_seed_FieldDecimal_hpp__
#include "fon9/seed/FieldValueFmt.hpp"
#include "fon9/Decimal.hpp"

namespace fon9 { namespace seed {

template <typename I>
StrView FieldDecimal_TypeId(const Field& fld, NumOutBuf& buf) {
   buf.SetEOS();
   char* pbeg = UIntToStrRev(buf.end(), fld.DecScale_);
   *--pbeg = '.';
   pbeg = UIntToStrRev(pbeg, fld.Size_);
   *--pbeg = (std::is_signed<I>::value ? 'S' : 'U');
   return StrView{pbeg, buf.end()};
}

/// \ingroup seed
/// 在設計階段就決定小數位的欄位: Decimal<I,S>;
template <typename I, DecScaleT S>
class FieldDecimal : public FieldValueFmt<Decimal<I, S>> {
   fon9_NON_COPY_NON_MOVE(FieldDecimal);
   using base = FieldValueFmt<Decimal<I, S>>;
public:
   /// 建構:
   /// - 固定為 FieldType::Decimal; FieldSource::DataMember;
   FieldDecimal(Named&& named, int32_t ofs) : base{std::move(named), FieldType::Decimal, ofs, S} {
   }
   /// 建構:
   /// - 固定為 FieldType::Decimal; FieldSource::DyMem;
   FieldDecimal(Named&& named) : base(std::move(named), FieldType::Decimal, S) {
   }

   /// 傳回: "Sn.d" or "Un.d";  n = this->Size_;  d = this->DecScale_;
   virtual StrView GetTypeId(NumOutBuf& buf) const override {
      return FieldDecimal_TypeId<I>(*this, buf);
   }
};

template <typename I, DecScaleT S>
inline FieldSPT<FieldDecimal<I, S>> MakeField(Named&& named, int32_t ofs, Decimal<I, S>&) {
   return FieldSPT<FieldDecimal<I, S>>{new FieldDecimal<I, S>{std::move(named), ofs}};
}

template <typename I, DecScaleT S>
inline FieldSPT<FieldDecimal<I, S>> MakeField(Named&& named, int32_t ofs, const Decimal<I, S>&) {
   return FieldSPT<FieldDecimal<I, S>>{new FieldConst<FieldDecimal<I, S>>{std::move(named), ofs}};
}

//--------------------------------------------------------------------------//

/// \ingroup seed
/// 建立欄位時才決定小數位, 例如: 從設定檔載入[小數位數];
template <typename I>
class fon9_API FieldDecimalDyScale : public Field {
   fon9_NON_COPY_NON_MOVE(FieldDecimalDyScale);
   using base = Field;
   using ValueT = I;

   enum : ValueT {
      kOrigNull = DecimalAux<ValueT>::OrigNull,
   };
public:
   /// 建構:
   /// - 固定為 FieldType::Decimal; FieldSource::DyMem;
   FieldDecimalDyScale(Named&& named, DecScaleT scale)
      : base(std::move(named), FieldType::Decimal, FieldSource::DyMem, 0, sizeof(ValueT), scale) {
   }

   ValueT GetOrigValue(const RawRd& rd) const {
      ValueT tmpValue;
      return rd.GetCellValue(*this, tmpValue);
   }
   void PutOrigValue(const RawWr& wr, ValueT value) const {
      wr.PutCellValue(*this, value);
   }

   /// 傳回: "Sn.d" or "Un.d";  n = this->Size_;  d = this->DecScale_;
   virtual StrView GetTypeId(NumOutBuf& buf) const override {
      return FieldDecimal_TypeId<I>(*this, buf);
   }

   virtual void CellRevPrint(const RawRd& rd, StrView fmt, RevBuffer& out) const override {
      IntScale<ValueT> value{this->GetOrigValue(rd), this->DecScale_};
      if (fon9_LIKELY(value.OrigValue_ != this->kOrigNull))
         FmtRevPrint(fmt, out, value);
   }
   virtual OpResult StrToCell(const RawWr& wr, StrView value) const override {
      this->PutOrigValue(wr, StrToDec(value, this->DecScale_, static_cast<ValueT>(this->kOrigNull)));
      return OpResult::no_error;
   }

   virtual FieldNumberT GetNumber(const RawRd& rd, DecScaleT outDecScale, FieldNumberT nullValue) const override {
      ValueT value = this->GetOrigValue(rd);
      if (fon9_LIKELY(value != this->kOrigNull))
         return static_cast<FieldNumberT>(AdjustDecScale(value, this->DecScale_, outDecScale));
      return nullValue;
   }
   virtual OpResult PutNumber(const RawWr& wr, FieldNumberT num, DecScaleT decScale) const override {
      this->PutOrigValue(wr, static_cast<ValueT>(AdjustDecScale(num, decScale, this->DecScale_)));
      return OpResult::no_error;
   }

   virtual OpResult SetNull(const RawWr& wr) const override {
      this->PutOrigValue(wr, this->kOrigNull);
      return OpResult::no_error;
   }
   virtual bool IsNull(const RawRd& rd) const override {
      return this->GetOrigValue(rd) == this->kOrigNull;
   }

   virtual OpResult Copy(const RawWr& wr, const RawRd& rd) const override {
      this->PutOrigValue(wr, this->GetOrigValue(rd));
      return OpResult::no_error;
   }
   virtual int Compare(const RawRd& lhs, const RawRd& rhs) const override {
      auto L = this->GetOrigValue(lhs);
      auto R = this->GetOrigValue(rhs);
      return (L < R) ? -1 : (L == R) ? 0 : 1;
   }
};

} } // namespaces
#endif//__fon9_seed_FieldDecimal_hpp__
