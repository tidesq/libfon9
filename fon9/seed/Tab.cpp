/// \file fon9/seed/Tab.cpp
/// \author fonwinz@gmail.com
#include "fon9/seed/Tab.hpp"
#include "fon9/Blob.h"

namespace fon9 { namespace seed {

uint32_t CalcDyMemSize(Fields& flds, Tab* tab, Field* keyfld) {
   *const_cast<uint32_t*>(&tab->DyBlobCount_) = 0;
   struct DySizeCalc {
      int32_t Size_ = 0;
      void AdjustBlobField(Field* fld, Tab* tab) {
         if (fld->Source_ == FieldSource::DyBlob) {
            // 這裡分配的 blob 空間, 在 ~Raw() 釋放.
            ++(*const_cast<uint32_t*>(&tab->DyBlobCount_));
            fld->Offset_ = this->Size_;
            this->Size_ += static_cast<int32_t>(sizeof(fon9_Blob));
         }
      }
      void AdjustField(Field* fld) {
         if (fld->Source_ == FieldSource::DyMem) {
            fld->Offset_ = this->Size_;
            this->Size_ += fld->Size_;
         }
      }
   };
   DySizeCalc dySizeCalc;
   if (keyfld)
      dySizeCalc.AdjustBlobField(keyfld, tab);
   // DyBlob 欄位的空間必須放在最前面.
   // - 因為 Raw 只有 uint32_t DyMemPos_, DyMemSize_;
   // - 因此必須把要「解構時需額外處理」的欄位, 集中在 DyMemPos_ 的前方。
   Field* fld;
   size_t L = 0;
   while ((fld = flds.Fields_.Get(L++)) != nullptr) {
      fld->Tab_ = tab;
      dySizeCalc.AdjustBlobField(fld, tab);
   }
   // 一般 [DyMem欄位(int,decimal,chars...)] 的空間必須放在 DyBlob 之後.
   if (keyfld)
      dySizeCalc.AdjustField(keyfld);
   L = 0;
   while ((fld = flds.Fields_.Get(L++)) != nullptr)
      dySizeCalc.AdjustField(fld);
   return static_cast<uint32_t>(dySizeCalc.Size_);
}

fon9_MSC_WARN_DISABLE(4355);
Tab::Tab(const Named& named, Fields&& fields, LayoutSP saplingLayout, TabFlag flags, Field* keyfld)
   : NamedIx{named}
   , Flags_{flags}
   , DyBlobCount_{}
   , DyMemSize_{CalcDyMemSize(fields, this, keyfld)}
   , Fields_{std::move(fields)}
   , SaplingLayout_{std::move(saplingLayout)} {
}
fon9_MSC_WARN_POP;

Tab::~Tab() {
}

} } // namespaces
