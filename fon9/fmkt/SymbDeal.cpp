// \file fon9/fmkt/SymbDeal.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbDeal.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 { namespace fmkt {

seed::Fields SymbDeal::MakeFields() {
   seed::Fields flds;
   flds.Add(fon9_MakeField(Named{"Time"},     SymbDeal, Data_.Time_));
   flds.Add(fon9_MakeField(Named{"DealPri"},  SymbDeal, Data_.Deal_.Pri_));
   flds.Add(fon9_MakeField(Named{"DealQty"},  SymbDeal, Data_.Deal_.Qty_));
   flds.Add(fon9_MakeField(Named{"TotalQty"}, SymbDeal, Data_.TotalQty_));
   return flds;
}

SymbDealTabDy::SymbDataSP SymbDealTabDy::FetchSymbData(Symb&) {
   return SymbDataSP{new SymbDeal{}};
}

} } // namespaces
