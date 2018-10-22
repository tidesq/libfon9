/// \file fon9/seed/ConfigGridView.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_ConfigGridView_hpp__
#define __fon9_seed_ConfigGridView_hpp__
#include "fon9/seed/Tab.hpp"
#include "fon9/seed/RawRd.hpp"

namespace fon9 { namespace seed {

fon9_API void RevPrintConfigFieldNames(RevBuffer& rbuf, const Fields& flds, StrView keyFieldName, FieldFlag excludes = FieldFlag::Readonly);
fon9_API void RevPrintConfigFieldValues(RevBuffer& rbuf, const Fields& flds, const RawRd& rd, FieldFlag excludes = FieldFlag::Readonly);

/// 一個建立 Config GridView 的範例.
template <class Iterator>
void RevPrintConfigGridView(RevBuffer& rbuf, const Fields& flds, Iterator ibeg, Iterator iend) {
   while (ibeg != iend) {
      RevPrintConfigFieldValues(rbuf, flds, SimpleRawRd{*--iend});
      RevPrint(rbuf, *fon9_kCSTR_ROWSPL, GetKey(*iend));
   }
}

/// line 1: Field name lists
/// line 2..: Grid view
struct fon9_API GridViewToContainer {
   virtual ~GridViewToContainer();

   std::vector<const Field*> Fields_;
   
   /// Parse first line for field names.
   /// - Skip first field name(Key field name).
   /// - 其餘從 tabflds 取出, 填入 this->Fields_
   void ParseFieldNames(const Fields& tabflds, StrView& cfgstr);
   /// line 1:   Field names
   /// line 2..: Grid view
   void ParseConfigStr(const Fields& tabflds, StrView cfgstr) {
      this->ParseFieldNames(tabflds, cfgstr);
      this->ParseConfigStr(cfgstr);
   }
   void ParseConfigStr(StrView cfgstr);
   void FillRaw(const RawWr& wr, StrView ln);

   /// 增加一筆新資料, 可透過 FillRaw(); 協助填入欄位內容.
   virtual void OnNewRow(StrView keyText, StrView ln) = 0;
};

} } // namespaces
#endif//__fon9_seed_ConfigGridView_hpp__
