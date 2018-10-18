// \file fon9/seed/ConfigGridView.cpp
// \author fonwinz@gmail.com
#include "fon9/seed/ConfigGridView.hpp"
#include "fon9/RevPrint.hpp"

namespace fon9 { namespace seed {

fon9_API void RevPrintConfigFieldNames(RevBuffer& rbuf, const Fields& flds, StrView keyFieldName) {
   size_t fldno = flds.size();
   while (fldno > 0) {
      const Field* fld = flds.Get(--fldno);
      if (!IsEnumContains(fld->Flags_, FieldFlag::Readonly))
         RevPrint(rbuf, *fon9_kCSTR_CELLSPL, fld->Name_);
   }
   RevPrint(rbuf, keyFieldName);
}
fon9_API void RevPrintConfigFieldValues(RevBuffer& rbuf, const Fields& flds, const RawRd& rd) {
   size_t fldno = flds.size();
   while (fldno > 0) {
      const Field* fld = flds.Get(--fldno);
      if (!IsEnumContains(fld->Flags_, FieldFlag::Readonly)) {
         fld->CellRevPrint(rd, nullptr, rbuf);
         RevPutChar(rbuf, *fon9_kCSTR_CELLSPL);
      }
   }
}

GridViewToContainer::~GridViewToContainer() {
}
void GridViewToContainer::ParseFieldNames(const Fields& tabflds, StrView& cfgstr) {
   this->Fields_.clear();
   this->Fields_.reserve(tabflds.size());
   StrView ln = StrFetchNoTrim(cfgstr, *fon9_kCSTR_ROWSPL);
   StrFetchNoTrim(ln, *fon9_kCSTR_CELLSPL);// skip key field name.
   while (!ln.empty())
      this->Fields_.push_back(tabflds.Get(StrFetchNoTrim(ln, *fon9_kCSTR_CELLSPL)));
}
void GridViewToContainer::ParseConfigStr(StrView cfgstr) {
   unsigned lineNo = 1;
   while (!cfgstr.empty()) {
      ++lineNo;
      StrView ln = StrFetchNoTrim(cfgstr, *fon9_kCSTR_ROWSPL);
      if (ln.empty())
         continue;
      StrView keyText = StrFetchNoTrim(ln, *fon9_kCSTR_CELLSPL);
      this->OnNewRow(keyText, ln);
   }
}
void GridViewToContainer::FillRaw(const RawWr& wr, StrView ln) {
   for (const Field* fld : this->Fields_) {
      if (ln.empty())
         break;
      StrView fldval = StrFetchNoTrim(ln, *fon9_kCSTR_CELLSPL);
      if (fld)
         fld->StrToCell(wr, fldval);
   }
}

} } // namespaces
