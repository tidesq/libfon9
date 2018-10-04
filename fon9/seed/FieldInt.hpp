/// \file fon9/seed/FieldInt.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_FieldInt_hpp__
#define __fon9_seed_FieldInt_hpp__
#include "fon9/seed/FieldChars.hpp"
#include "fon9/Unaligned.hpp"
#include "fon9/StrTo.hpp"

namespace fon9 { namespace seed {

template <bool isSigned, size_t sz> struct FieldIntTypeId;
template <> struct FieldIntTypeId<true, 1> { static constexpr StrView TypeId() { return StrView{"S1"}; } };
template <> struct FieldIntTypeId<true, 2> { static constexpr StrView TypeId() { return StrView{"S2"}; } };
template <> struct FieldIntTypeId<true, 4> { static constexpr StrView TypeId() { return StrView{"S4"}; } };
template <> struct FieldIntTypeId<true, 8> { static constexpr StrView TypeId() { return StrView{"S8"}; } };
template <> struct FieldIntTypeId<false, 1> { static constexpr StrView TypeId() { return StrView{"U1"}; } };
template <> struct FieldIntTypeId<false, 2> { static constexpr StrView TypeId() { return StrView{"U2"}; } };
template <> struct FieldIntTypeId<false, 4> { static constexpr StrView TypeId() { return StrView{"U4"}; } };
template <> struct FieldIntTypeId<false, 8> { static constexpr StrView TypeId() { return StrView{"U8"}; } };
template <typename IntT>
inline constexpr StrView GetFieldIntTypeId() {
   return FieldIntTypeId<std::is_signed<IntT>::value, sizeof(IntT)>::TypeId();
}

//--------------------------------------------------------------------------//

/// \ingroup seed
/// 存取[整數類型別]的欄位類別.
/// - Null = 0
template <class IntT>
class fon9_API FieldInt : public Field {
   fon9_NON_COPY_NON_MOVE(FieldInt);
   using base = Field;
   static_assert(std::is_integral<IntT>::value, "FieldInt<IntT>, IntT必須是整數型別");
protected:
   using base::base;

public:
   using OrigType = IntT;
   enum : IntT {
      kNullValue = IntT{}
   };

   /// 建構:
   /// - 固定為 FieldType::Integer; FieldSource::DataMember;
   FieldInt(Named&& named, int32_t ofs)
      : base(std::move(named), FieldType::Integer, FieldSource::DataMember, ofs, sizeof(IntT), 0) {
   }
   /// 建構:
   /// - 固定為 FieldType::Integer; FieldSource::DyMem;
   FieldInt(Named&& named)
      : base(std::move(named), FieldType::Integer, FieldSource::DyMem, 0, sizeof(IntT), 0) {
   }

   IntT GetValue(const RawRd& rd) const {
      IntT tmpValue;
      return rd.GetCellValue(*this, tmpValue);
   }
   void PutValue(const RawWr& wr, IntT value) const {
      wr.PutCellValue(*this, value);
   }

   static constexpr StrView TypeId() {
      return GetFieldIntTypeId<IntT>();
   }
   /// 傳回: "Sn" or "Un";  n = this->Size_;
   virtual StrView GetTypeId(NumOutBuf&) const override {
      return TypeId();
   }

   virtual void CellRevPrint(const RawRd& rd, StrView fmt, RevBuffer& out) const override {
      FmtRevPrint(fmt, out, this->GetValue(rd));
   }
   virtual OpResult StrToCell(const RawWr& wr, StrView value) const override {
      this->PutValue(wr, StrTo(value, static_cast<IntT>(kNullValue)));
      return OpResult::no_error;
   }

   virtual FieldNumberT GetNumber(const RawRd& rd, DecScaleT outDecScale, FieldNumberT nullValue) const override {
      (void)nullValue;
      return static_cast<FieldNumberT>(AdjustDecScale(this->GetValue(rd), 0, outDecScale));
   }
   virtual OpResult PutNumber(const RawWr& wr, FieldNumberT num, DecScaleT decScale) const override {
      this->PutValue(wr, static_cast<IntT>(AdjustDecScale(num, decScale, 0)));
      return OpResult::no_error;
   }

   /// \retval true  整數值==kNullValue;
   virtual bool IsNull(const RawRd& rd) const override {
      return this->GetValue(rd) == static_cast<IntT>(kNullValue);
   }
   virtual OpResult SetNull(const RawWr& wr) const override {
      this->PutValue(wr, static_cast<IntT>(kNullValue));
      return OpResult::no_error;
   }

   virtual OpResult Copy(const RawWr& wr, const RawRd& rd) const override {
      this->PutValue(wr, this->GetValue(rd));
      return OpResult::no_error;
   }
   virtual int Compare(const RawRd& lhs, const RawRd& rhs) const override {
      auto L = this->GetValue(lhs);
      auto R = this->GetValue(rhs);
      return (L < R) ? -1 : (L == R) ? 0 : 1;
   }
};

template class FieldInt<int8_t> fon9_API;
template class FieldInt<int16_t> fon9_API;
template class FieldInt<int32_t> fon9_API;
template class FieldInt<int64_t> fon9_API;
template class FieldInt<uint8_t> fon9_API;
template class FieldInt<uint16_t> fon9_API;
template class FieldInt<uint32_t> fon9_API;
template class FieldInt<uint64_t> fon9_API;

template <typename IntT, class FieldT = FieldInt<decay_t<IntT>>>
inline enable_if_t<std::is_integral<IntT>::value, FieldSPT<FieldT>>
MakeField(Named&& named, int32_t ofs, IntT&) {
   return FieldSPT<FieldT>{new FieldT{std::move(named), ofs}};
}

template <typename IntT, class FieldT = FieldInt<decay_t<IntT>>>
inline enable_if_t<std::is_integral<IntT>::value, FieldSPT<FieldT>>
MakeField(Named&& named, int32_t ofs, const IntT&) {
   return FieldSPT<FieldT>{new FieldConst<FieldT>{std::move(named), ofs}};
}

//--------------------------------------------------------------------------//

template <bool isSigned, size_t sz> struct FieldIntHexTypeId;
template <> struct FieldIntHexTypeId<true, 1> { static constexpr StrView TypeId() { return StrView{"S1x"}; } };
template <> struct FieldIntHexTypeId<true, 2> { static constexpr StrView TypeId() { return StrView{"S2x"}; } };
template <> struct FieldIntHexTypeId<true, 4> { static constexpr StrView TypeId() { return StrView{"S4x"}; } };
template <> struct FieldIntHexTypeId<true, 8> { static constexpr StrView TypeId() { return StrView{"S8x"}; } };
template <> struct FieldIntHexTypeId<false, 1> { static constexpr StrView TypeId() { return StrView{"U1x"}; } };
template <> struct FieldIntHexTypeId<false, 2> { static constexpr StrView TypeId() { return StrView{"U2x"}; } };
template <> struct FieldIntHexTypeId<false, 4> { static constexpr StrView TypeId() { return StrView{"U4x"}; } };
template <> struct FieldIntHexTypeId<false, 8> { static constexpr StrView TypeId() { return StrView{"U8x"}; } };
template <typename IntT>
inline constexpr StrView GetFieldIntHexTypeId() {
   return FieldIntHexTypeId<std::is_signed<IntT>::value, sizeof(IntT)>::TypeId();
}

template <typename IntT>
class FieldIntHex : public FieldInt<IntT> {
   fon9_NON_COPY_NON_MOVE(FieldIntHex);
   using base = FieldInt<IntT>;
public:
   using base::base;

   static constexpr StrView TypeId() {
      return GetFieldIntHexTypeId<IntT>();
   }
   /// 傳回: "Snx" or "Unx";  n = this->Size_;
   virtual StrView GetTypeId(NumOutBuf&) const override {
      return TypeId();
   }

   virtual void CellRevPrint(const RawRd& rd, StrView fmt, RevBuffer& out) const override {
      RevPrint(out, ToHex{this->GetValue(rd)}, fmt);
      const unsigned char* pcur = reinterpret_cast<const unsigned char*>(out.GetCurrent());
      char ch = static_cast<char>(toupper(pcur[0]));
      if ((ch != 'X') && (ch != '0' || toupper(pcur[1]) != 'X'))
         RevPutChar(out, 'x');
   }
   virtual OpResult StrToCell(const RawWr& wr, StrView value) const override {
      this->PutValue(wr, static_cast<IntT>(HIntStrTo(value)));
      return OpResult::no_error;
   }
};

/// \ingroup seed
/// 若 EnumT 有提供 bit op, 則使用 FieldIntHex 欄位.
/// 若 EnumT 沒提供 bit op, 則使用 FieldInt 欄位.
template <typename EnumT, typename IntT = typename std::underlying_type<EnumT>::type>
struct FieldEnumAux {
   using Field = conditional_t<HasBitOpT<EnumT>::value, FieldIntHex<IntT>, FieldInt<IntT>>;
   using FieldSP = FieldSPT<Field>;
};

/// \ingroup seed
/// 若 EnumT 為 enum : char, 則使用 FieldChar1 欄位.
template <typename EnumT>
struct FieldEnumAux<EnumT, char> {
   using Field = FieldChar1;
   using FieldSP = FieldSPT<Field>;
};

template <typename EnumT>
struct FieldEnumSelectorAux : public FieldEnumAux<EnumT> {
};

template <typename EnumT>
struct FieldEnumSelector : public conditional_t<std::is_enum<EnumT>::value, FieldEnumSelectorAux<EnumT>, std::false_type> {
};

template <typename EnumT, class Selector = FieldEnumSelector<EnumT>>
inline typename Selector::FieldSP MakeField(Named&& named, int32_t ofs, EnumT&) {
   return typename Selector::FieldSP{new typename Selector::Field{std::move(named), ofs}};
}

template <typename EnumT, class Selector = FieldEnumSelector<EnumT>>
inline typename Selector::FieldSP MakeField(Named&& named, int32_t ofs, const EnumT&) {
   return typename Selector::FieldSP{new FieldConst<typename Selector::Field>{std::move(named), ofs}};
}

} } // namespaces
#endif//__fon9_seed_FieldInt_hpp__
