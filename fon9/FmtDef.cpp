/// \file fon9/FmtDef.cpp
/// \author fonwinz@gmail.com
#include "fon9/FmtDef.hpp"
#include "fon9/StrTo.hpp"

namespace fon9 {

FmtDef::FmtDef(StrView fmtstr) {
   for (const char& chFlag : fmtstr) {
      switch (static_cast<FmtChar>(chFlag)) {
         #define case_FmtChar_FmtFlag(flgName) \
         case FmtChar::flgName: this->Flags_ |= FmtFlag::flgName; break;
         static_assert(FmtChar::ShowPrefix == FmtChar::ForceDecimalPoint,
                       "FmtChar::ShowPrefix should equal FmtChar::ForceDecimalPoint?");
         case_FmtChar_FmtFlag(LeftJustify);
         case_FmtChar_FmtFlag(ShowPlus0);
         case_FmtChar_FmtFlag(ShowSign);
         case_FmtChar_FmtFlag(IntPad0);
         case_FmtChar_FmtFlag(ShowPrefix);
         case_FmtChar_FmtFlag(ShowIntSep);
         default:
            this->ParseWidth(&chFlag, fmtstr.end());
            return;
      }
   }
}
void FmtDef::ParseWidth(const char* pcur, const char* pend) {
   this->Width_ = static_cast<WidthType>(NaiveStrToUInt(pcur, pend, &pcur));
   if (pcur == pend)
      return;
   switch (*pcur) {
   case '_':
      this->Flags_ |= FmtFlag::NoDecimalPoint;
      // 不用 break; 繼續解析 this->Precision_;
   case '.':
      this->Flags_ |= FmtFlag::HasPrecision;
      if (++pcur == pend)
         this->Precision_ = 0;
      else
         this->Precision_ = static_cast<WidthType>(NaiveStrToUInt(pcur, pend));
      return;
   }
   return;
}

} // namespace
