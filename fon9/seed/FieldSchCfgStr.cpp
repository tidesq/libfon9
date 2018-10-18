/// \file fon9/seed/FieldSchCfgStr.cpp
/// \author fonwinz@gmail.com
#include "fon9/seed/FieldSchCfgStr.hpp"
#include "fon9/seed/FieldDyBlob.hpp"

namespace fon9 { namespace seed {

static FieldSP SchCfgStrFieldMaker(StrView& fldcfg, char chSpl, char chTail) {
   Named named{DeserializeNamed(fldcfg, chSpl, chTail)};
   if (named.Name_.empty())
      return FieldSP{};
   using FieldSchCfgStr_Dy = FieldSchCfgStrT<FieldDyBlob>;
   return FieldSP{new FieldSchCfgStr_Dy(std::move(named), FieldType::Chars)};
}

static FieldMakerRegister reg{StrView{fon9_kCSTR_UDFieldMaker_SchCfgStr}, &SchCfgStrFieldMaker};

} } // namespaces
