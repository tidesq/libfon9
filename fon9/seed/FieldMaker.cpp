/// \file fon9/seed/FieldMaker.cpp
/// \author fonwinz@gmail.com
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/seed/FieldDyBlob.hpp"

namespace fon9 { namespace seed {

static size_t StrToCountNum(StrView& fldcfg) {
   const char* pend;
   size_t      count = StrTo(fldcfg, 0u, &pend);
   fldcfg.SetBegin(pend);
   return count;
}

static FieldSP MakeFieldChars(StrView& fldcfg, char chSpl, char chTail) {
   size_t   byteCount = StrToCountNum(fldcfg);
   Named    named{DeserializeNamed(fldcfg, chSpl, chTail)};
   if (named.Name_.empty())
      return FieldSP{};
   switch (byteCount) {
   case 0:  return FieldSP{new FieldDyBlob{std::move(named), FieldType::Chars}};
   case 1:  return FieldSP{new FieldChar1{std::move(named)}};
   default: return FieldSP{new FieldChars{std::move(named), byteCount}};
   }
}
static FieldSP MakeFieldBytes(StrView& fldcfg, char chSpl, char chTail) {
   size_t   byteCount = StrToCountNum(fldcfg);
   Named    named{DeserializeNamed(fldcfg, chSpl, chTail)};
   if (named.Name_.empty())
      return FieldSP{};
   if (byteCount == 0)
      return FieldSP{new FieldDyBlob{std::move(named), FieldType::Bytes}};
   return FieldSP{new FieldBytes{std::move(named), byteCount}};
}

//--------------------------------------------------------------------------//

static FieldSP MakeFieldTime(StrView& fldcfg, char chSpl, char chTail) {
   int chSubType = fldcfg.Get1st();
   if (chSubType == EOF)
      return FieldSP{};
   const char* perr = fldcfg.begin();
   fldcfg.SetBegin(perr + 1);

   Named named{DeserializeNamed(fldcfg, chSpl, chTail)};
   if (named.Name_.empty())
      return FieldSP{};
   switch (chSubType) {
   case 'i': // Ti = TimeInterval.
      return FieldSP{new FieldTimeInterval{std::move(named)}};
   case 's': // Ts = TimeStamp.
      return FieldSP{new FieldTimeStamp{std::move(named)}};
   }
   fldcfg.SetBegin(perr);
   return FieldSP{};
}

//--------------------------------------------------------------------------//

struct MakeField_SelSigned {
   using I1 = int8_t;
   using I2 = int16_t;
   using I4 = int32_t;
   using I8 = int64_t;
};
struct MakeField_SelUnsigned {
   using I1 = uint8_t;
   using I2 = uint16_t;
   using I4 = uint32_t;
   using I8 = uint64_t;
};

template <class BSelInt>
static FieldSP MakeFieldInt(Named&& named, size_t byteCount) {
   switch (byteCount) {
   case 1:  return FieldSP{new FieldInt<typename BSelInt::I1>{std::move(named)}};
   case 2:  return FieldSP{new FieldInt<typename BSelInt::I2>{std::move(named)}};
   case 4:  return FieldSP{new FieldInt<typename BSelInt::I4>{std::move(named)}};
   case 8:  return FieldSP{new FieldInt<typename BSelInt::I8>{std::move(named)}};
   }
   return FieldSP{};
}
template <class BSelInt>
static FieldSP MakeFieldIntHex(Named&& named, size_t byteCount) {
   switch (byteCount) {
   case 1:  return FieldSP{new FieldIntHex<typename BSelInt::I1>{std::move(named)}};
   case 2:  return FieldSP{new FieldIntHex<typename BSelInt::I2>{std::move(named)}};
   case 4:  return FieldSP{new FieldIntHex<typename BSelInt::I4>{std::move(named)}};
   case 8:  return FieldSP{new FieldIntHex<typename BSelInt::I8>{std::move(named)}};
   }
   return FieldSP{};
}
template <class BSelInt>
static FieldSP MakeFieldDecimal(Named&& named, size_t byteCount, DecScaleT scale) {
   switch (byteCount) {
   case 1:  return FieldSP{new FieldDecimalDyScale<typename BSelInt::I1>{std::move(named), scale}};
   case 2:  return FieldSP{new FieldDecimalDyScale<typename BSelInt::I2>{std::move(named), scale}};
   case 4:  return FieldSP{new FieldDecimalDyScale<typename BSelInt::I4>{std::move(named), scale}};
   case 8:  return FieldSP{new FieldDecimalDyScale<typename BSelInt::I8>{std::move(named), scale}};
   }
   return FieldSP{};
}

//--------------------------------------------------------------------------//

static FieldSP MakeFieldImpl(StrView& fldcfg, char chSpl, char chTail) {
   StrTrimHead(&fldcfg);
   int chType = fldcfg.Get1st();
   if (chType == EOF)
      return FieldSP{};
   fldcfg.SetBegin(fldcfg.begin() + 1);
   switch (chType) {
   default:
      fldcfg.SetBegin(fldcfg.begin() - 1);
      break;
   case 'C':   return MakeFieldChars(fldcfg, chSpl, chTail);
   case 'B':   return MakeFieldBytes(fldcfg, chSpl, chTail);
   case 'T':   return MakeFieldTime(fldcfg, chSpl, chTail);
   case 'S':
   case 'U':
      const char* perr = fldcfg.begin();
      size_t      byteCount = StrToCountNum(fldcfg);
      if (byteCount <= 0)
         break;
      DecScaleT scale;
      switch (fldcfg.Get1st()) {
      default:
         scale = static_cast<DecScaleT>(-1);
         break;
      case 'x':// hex.
         fldcfg.SetBegin(fldcfg.begin() + 1);
         scale = static_cast<DecScaleT>(-2);
         break;
      case '.':
         fldcfg.SetBegin(fldcfg.begin() + 1);
         scale = static_cast<DecScaleT>(StrToCountNum(fldcfg));
         break;
      }
      Named named{DeserializeNamed(fldcfg, chSpl, chTail)};
      if (named.Name_.empty())
         break;
      FieldSP fld;
      switch (scale) {
      case static_cast<DecScaleT>(-1):
         fld = (chType == 'S'
                ? MakeFieldInt<MakeField_SelSigned>(std::move(named), byteCount)
                : MakeFieldInt<MakeField_SelUnsigned>(std::move(named), byteCount));
         break;
      case static_cast<DecScaleT>(-2):
         fld = (chType == 'S'
                ? MakeFieldIntHex<MakeField_SelSigned>(std::move(named), byteCount)
                : MakeFieldIntHex<MakeField_SelUnsigned>(std::move(named), byteCount));
         break;
      default:
         fld = (chType == 'S' // 允許小數位=0, 此欄位支援 Null.
                ? MakeFieldDecimal<MakeField_SelSigned>(std::move(named), byteCount, scale)
                : MakeFieldDecimal<MakeField_SelUnsigned>(std::move(named), byteCount, scale));
         break;
      }
      if (fld)
         return fld;
      fldcfg.SetBegin(perr);
      break;
   }
   return FieldSP{};
}

fon9_API FieldSP MakeField(StrView& fldcfg, char chSpl, char chTail) {
   if (StrTrimHead(&fldcfg).empty())
      return FieldSP{};
   FieldFlag fldFlags{};
   if (fldcfg.Get1st() == '(') { // (FieldFlags)
      for(;;) {
         fldcfg.SetBegin(fldcfg.begin() + 1);
         switch (int ch = fldcfg.Get1st()) {
         case static_cast<char>(FieldFlagChar::Readonly): fldFlags |= FieldFlag::Readonly; break;
         case static_cast<char>(FieldFlagChar::Hide):     fldFlags |= FieldFlag::Hide;     break;
         case ')':
            fldcfg.SetBegin(fldcfg.begin() + 1);
            goto __MAKE_FIELD_IMPL;
         default:
            if (isspace(ch))
               break;
            // 非空白的無效字元, 返回失敗.
         case EOF:
            return FieldSP{};
         }
      }
   }
__MAKE_FIELD_IMPL:
   FieldSP fld = MakeFieldImpl(fldcfg, chSpl, chTail);
   if (fld && fldFlags != FieldFlag{})
      fld->Flags_ = fldFlags;
   return fld;
}

OpResult FieldsMaker::OnFieldExProperty(Field&, StrView) {
   return OpResult::not_supported_cmd;
}

OpResult FieldsMaker::Parse(StrView& fldcfg, char chSpl, char chTail, Fields& fields) {
   while(!StrTrimHead(&fldcfg).empty()) {
      if (fon9_UNLIKELY(fldcfg.Get1st() == chTail)) {
         fldcfg.SetBegin(fldcfg.begin() + 1);
         continue;
      }
      this->LastLinePos_ = fldcfg.begin();
      StrView exProp;
      if (fldcfg.Get1st() == '{') {// {exProp}
         fldcfg.SetBegin(fldcfg.begin() + 1);
         exProp = SbrFetchNoTrim(fldcfg, '}');
         StrTrimHead(&fldcfg);
         StrTrim(&exProp);
      }
      FieldSP nfld = MakeField(fldcfg, chSpl, chTail);
      if (!nfld)
         return OpResult::value_format_error;
      Field* cfld = fields.Get(&nfld->Name_);
      if (cfld == nullptr) {
         cfld = nfld.get();
         fields.Add(std::move(nfld));
      }
      else {
         NumOutBuf buf1, buf2;
         if (cfld->GetTypeId(buf1) != nfld->GetTypeId(buf2))
            return OpResult::key_exists;
         if (!nfld->GetTitle().empty())
            cfld->SetTitle(nfld->GetTitle());
         if (!nfld->GetDescription().empty())
            cfld->SetDescription(nfld->GetDescription());
      }
      if (!exProp.empty()) {
         OpResult res = this->OnFieldExProperty(*cfld, exProp);
         if (res != OpResult::no_error)
            return res;
      }
   }
   return OpResult::no_error;
}

//--------------------------------------------------------------------------//

fon9_API void AppendFieldConfig(std::string& fldcfg, const Field& field, char chSpl, int chTail) {
   FieldFlag flags = field.Flags_;
   if (flags != FieldFlag{}) {
      fldcfg.push_back('(');
      //--------
      #define PUSH_BACK_FIELD_FLAG_CHAR(fldcfg, flags, item) \
            if (IsEnumContains(flags, FieldFlag::item)) \
               fldcfg.push_back(static_cast<char>(FieldFlagChar::item))
         //--------
      PUSH_BACK_FIELD_FLAG_CHAR(fldcfg, flags, Readonly);
      PUSH_BACK_FIELD_FLAG_CHAR(fldcfg, flags, Hide);
      fldcfg.push_back(')');
      fldcfg.push_back(' ');
   }
   fon9::NumOutBuf nbuf;
   field.GetTypeId(nbuf).AppendTo(fldcfg);
   fldcfg.push_back(' ');
   SerializeNamed(fldcfg, field, chSpl, chTail);
}

fon9_API void AppendFieldsConfig(std::string& fldcfg, const Fields& fields, char chSpl, char chTail) {
   size_t L = 0;
   while (const Field* fld = fields.Get(L++))
      AppendFieldConfig(fldcfg, *fld, chSpl, chTail);
}

} } // namespaces
