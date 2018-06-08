/// \file fon9/seed/FieldString.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_FieldString_hpp__
#define __fon9_seed_FieldString_hpp__
#include "fon9/seed/Field.hpp"
#include "fon9/seed/RawWr.hpp"
#include "fon9/StrTo.hpp"
#include "fon9/RevPrint.hpp"
#include "fon9/CharVector.hpp"

namespace fon9 { namespace seed {

/// \ingroup seed
/// 存取 StringT member 的欄位.
/// \tparam StringT 必須支援:
///   - StrView ToStrView(const StringT&);
///   - StringT::clear();
///   - StringT::assign(const char* pbeg, const char* pend);
///   - bool StringT::empty() const;
///   - StringT& StringT::operator=(const StringT&);
template <class StringT>
class fon9_API FieldString : public Field {
   fon9_NON_COPY_NON_MOVE(FieldString);
   typedef Field  base;
public:
   /// 建構, 固定為:
   /// - FieldType::Chars;
   /// - StringT 只能是 data member(不可是 DyMem), 所以 FieldSource 固定為 DataMember;
   FieldString(Named&& named, int32_t ofs)
      : base{std::move(named), FieldType::Chars, FieldSource::DataMember, ofs, sizeof(StringT), 0} {
   }

   StrView GetValue(const RawRd& rd) const {
      return ToStrView(rd.GetMemberCell<StringT>(*this));
   }
   StrView GetStrView(const RawRd& rd) const {
      return this->GetValue(rd);
   }

   /// 傳回: "C0";
   virtual StrView GetTypeId(NumOutBuf&) const override {
      return StrView{"C0"};
   }

   virtual void CellRevPrint(const RawRd& rd, StrView fmt, RevBuffer& out) const override {
      FmtRevPrint(fmt, out, this->GetValue(rd));
   }

   virtual OpResult StrToCell(const RawWr& wr, StrView value) const override {
      wr.GetMemberCell<StringT>(*this).assign(value.begin(), value.end());
      return OpResult::no_error;
   }

   virtual OpResult SetNull(const RawWr& wr) const override {
      wr.GetMemberCell<StringT>(*this).clear();
      return OpResult::no_error;
   }

   virtual bool IsNull(const RawRd& rd) const override {
      return rd.GetMemberCell<StringT>(*this).empty();
   }

   virtual FieldNumberT GetNumber(const RawRd& rd, DecScaleT outDecScale, FieldNumberT nullValue) const override {
      return StrToDec(this->GetValue(rd), outDecScale, nullValue);
   }

   virtual OpResult PutNumber(const RawWr& wr, FieldNumberT num, DecScaleT decScale) const override {
      NumOutBuf nbuf;
      char*     pbeg = DecToStrRev(nbuf.end(), num, decScale);
      wr.GetMemberCell<StringT>(*this).assign(pbeg, nbuf.end());
      return OpResult::no_error;
   }

   virtual OpResult Copy(const RawWr& wr, const RawRd& rd) const override {
      wr.GetMemberCell<StringT>(*this) = rd.GetMemberCell<StringT>(*this);
      return OpResult::no_error;
   }
};

template <class StringT, class FieldT = FieldString<decay_t<StringT>>>
inline auto MakeField(Named&& named, int32_t ofs, StringT& dataMember)
-> decltype(ToStrView(dataMember), dataMember.empty(), FieldSPT<FieldT>{}) {
   (void)dataMember;
   return FieldSPT<FieldT>{new FieldT{std::move(named), ofs}};
}

template <class StringT, class FieldT = FieldString<decay_t<StringT>>>
inline auto MakeField(Named&& named, int32_t ofs, const StringT& dataMember)
-> decltype(ToStrView(dataMember), dataMember.empty(), FieldSPT<FieldT>{}) {
   (void)dataMember;
   return FieldSPT<FieldT>{new FieldConst<FieldT>{std::move(named), ofs}};
}

/// \ingroup seed
/// FieldStdString = 存取 std::string member 的欄位.
fon9_API_TEMPLATE_CLASS(FieldStdString, FieldString, std::string)

/// \ingroup seed
/// FieldCharVector = 存取 CharVector member 的欄位.
fon9_API_TEMPLATE_CLASS(FieldCharVector, FieldString, CharVector)

} } // namespaces
#endif//__fon9_seed_FieldString_hpp__
