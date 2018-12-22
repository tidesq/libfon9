// \file fon9/fmkt/Symb.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/Symb.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 { namespace fmkt {

SymbData::~SymbData() {
}

//--------------------------------------------------------------------------//

SymbData* Symb::GetSymbData(int tabid) {
   return tabid <= 0 ? this : nullptr;
}
SymbData* Symb::FetchSymbData(int tabid) {
   return tabid <= 0 ? this : nullptr;
}
seed::Fields Symb::MakeFields() {
   seed::Fields flds;
   flds.Add(fon9_MakeField(Named{"Market"},    Symb, TradingMarket_));
   flds.Add(fon9_MakeField(Named{"ShUnit"},    Symb, ShUnit_));
   flds.Add(fon9_MakeField(Named{"FlowGroup"}, Symb, FlowGroup_));
   return flds;
}

} } // namespaces
