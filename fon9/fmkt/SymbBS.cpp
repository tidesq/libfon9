// \file fon9/fmkt/SymbBS.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbBS.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 { namespace fmkt {

seed::Fields SymbBS::MakeFields() {
   seed::Fields flds;
   flds.Add(fon9_MakeField(Named{"Time"}, SymbBS, Data_.Time_));
   char bsPriName[] = "-nP";
   char bsQtyName[] = "-nQ";
   int  idx;
   bsPriName[0] = bsQtyName[0] = 'S';
   for (idx = kBSCount - 1; idx >= 0; --idx) {
      bsPriName[1] = bsQtyName[1] = static_cast<char>(idx + '1');
      flds.Add(fon9_MakeField(Named{bsPriName}, SymbBS, Data_.Sells_[idx].Pri_));
      flds.Add(fon9_MakeField(Named{bsQtyName}, SymbBS, Data_.Sells_[idx].Qty_));
   }
   bsPriName[0] = bsQtyName[0] = 'B';
   for (idx = 0; idx < kBSCount; ++idx) {
      bsPriName[1] = bsQtyName[1] = static_cast<char>(idx + '1');
      flds.Add(fon9_MakeField(Named{bsPriName}, SymbBS, Data_.Buys_[idx].Pri_));
      flds.Add(fon9_MakeField(Named{bsQtyName}, SymbBS, Data_.Buys_[idx].Qty_));
   }
   return flds;
}

SymbBSTabDy::SymbDataSP SymbBSTabDy::FetchSymbData(Symb&) {
   return SymbDataSP{new SymbBS{}};
}

} } // namespaces
