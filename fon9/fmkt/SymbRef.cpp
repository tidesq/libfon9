// \file fon9/fmkt/SymbRef.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbRef.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 { namespace fmkt {

seed::Fields SymbRef::MakeFields() {
   seed::Fields flds;
   flds.Add(fon9_MakeField(Named{"PriRef"},   SymbRef, PriRef_));
   flds.Add(fon9_MakeField(Named{"PriUpLmt"}, SymbRef, PriUpLmt_));
   flds.Add(fon9_MakeField(Named{"PriDnLmt"}, SymbRef, PriDnLmt_));
   return flds;
}

SymbRefTabDy::SymbDataSP SymbRefTabDy::FetchSymbData(Symb&) {
   return SymbDataSP{new SymbRef{}};
}

} } // namespaces
