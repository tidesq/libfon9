// \file fon9/ToStrFmt.cpp
// \author fonwinz@gmail.com
#include "fon9/ToStrFmt.hpp"

namespace fon9 {

fon9_API char* ToStrRev_Hide0(char* pout, FmtDef fmt) {
   if (fon9_LIKELY(!IsEnumContainsAny(fmt.Flags_, FmtFlag::MaskSign))) {
      if (fmt.Width_ > 0)
         return reinterpret_cast<char*>(memset(pout - fmt.Width_, ' ', fmt.Width_));
      return pout;
   }
   char chSign = (fmt.Flags_ & FmtFlag::MaskSign) == FmtFlag::ShowPlus0 ? '+' : ' ';
   if (fmt.Width_ <= 1) {
      *--pout = chSign;
      return pout;
   }
   --fmt.Width_;
   if (IsEnumContains(fmt.Flags_, FmtFlag::LeftJustify)) {
      memset(pout -= fmt.Width_, ' ', fmt.Width_);
      *--pout = chSign;
   }
   else {
      *--pout = chSign;
      memset(pout -= fmt.Width_, ' ', fmt.Width_);
   }
   return pout;
}

static char* LeftJustify(char* pout, unsigned szout, size_t szfill) {
   memmove(pout - szfill, pout, szout);
   pout -= szfill;
   memset(pout + szout, ' ', szfill);
   return pout;
}

fon9_API char* IntToStrRev_LastJustify(char* pout, unsigned szout, bool isNeg, FmtDef fmt) {
   int szfill = static_cast<int>(fmt.Precision_ - szout);
   if (fon9_UNLIKELY(szfill > 0)) {
      memset(pout -= szfill, '0', static_cast<size_t>(szfill));
      szout += szfill;
   }
   char chSign;
   if (isNeg)
      chSign = '-';
   else {
      fon9_WARN_DISABLE_SWITCH;
      switch (fmt.Flags_ & FmtFlag::MaskSign) {
      case FmtFlag::ShowPlus0: chSign = '+';    break;
      case FmtFlag::ShowSign:  chSign = ' ';    break;
      default:                 chSign = '\0';   break;
      }
      fon9_WARN_POP;
   }
   szfill = static_cast<int>(fmt.Width_ - szout - (chSign != '\0'));
   if (szfill <= 0) {
      if (chSign)
         *--pout = chSign;
   }
   else {
      if (IsEnumContains(fmt.Flags_, FmtFlag::LeftJustify)) {
         if (chSign) {
            ++szout;
            *--pout = chSign;
         }
         return LeftJustify(pout, szout, static_cast<size_t>(szfill));
      }
      char chfill = (IsEnumContains(fmt.Flags_, FmtFlag::HasPrecision) ? ' '
                     : IsEnumContains(fmt.Flags_, FmtFlag::IntPad0) ? '0'
                     : ' ');
      if (chfill == '0' || chSign == '\0') {
         memset(pout -= szfill, chfill, static_cast<size_t>(szfill));
         if (chSign)
            *--pout = chSign;
      }
      else {
         *--pout = chSign;
         memset(pout -= szfill, chfill, static_cast<size_t>(szfill));
      }
   }
   return pout;
}

fon9_API char* IntToStrRev(char* pout, uintmax_t value, bool isNeg, FmtDef fmt) {
   if (fon9_UNLIKELY(value == 0)) {
      if (IsEnumContainsAny(fmt.Flags_, FmtFlag::Hide0)
          || (fmt.Flags_ & (FmtFlag::IntHide0 | FmtFlag::IntPad0)) == FmtFlag::IntHide0
          || (IsEnumContains(fmt.Flags_, FmtFlag::HasPrecision) && fmt.Precision_ == 0)) {
         return ToStrRev_Hide0(pout, fmt);
      }
   }
   char* const pstart = pout;
   pout = UIntToStrRev_CheckIntSep(pstart, value, fmt.Flags_);
   return IntToStrRev_LastJustify(pout, static_cast<unsigned>(pstart - pout), isNeg, fmt);
}

//--------------------------------------------------------------------------//

fon9_API char* ToStrRev_DecScalePrecision(char* pout, uintmax_t& value, DecScaleT scale, FmtDef::WidthType precision,
                                          uintmax_t (*fnRound)(uintmax_t val, uintmax_t div)) {
   uintmax_t vdec = value;
   int exdec = static_cast<int>(precision - scale);
   if (fon9_LIKELY(exdec == 0))
      exdec = scale;
   else if (fon9_LIKELY(exdec > 0)) {
      memset(pout -= exdec, '0', static_cast<size_t>(exdec));
      exdec = scale;
   }
   else {
      vdec = (*fnRound)(vdec, unsigned_cast(GetDecDivisor(static_cast<DecScaleT>(-exdec))));
      exdec = static_cast<int>(precision);
   }
   for (; exdec > 0; --exdec) {
      *--pout = static_cast<char>(vdec % 10 + '0');
      vdec /= 10;
   }
   value = vdec;
   return pout;
}

fon9_API char* ToStrRev_DecScale(char* pout, uintmax_t& value, DecScaleT scale, FmtDef fmt) {
   char* const pstart = pout;
   if (IsEnumContains(fmt.Flags_, FmtFlag::HasPrecision)) {
      pout = ToStrRev_DecScalePrecision(pout, value, scale, fmt.Precision_, [](uintmax_t val, uintmax_t div) {
         // 四捨五入.
         return (val + (div / 2)) / div;
      });
      if (!IsEnumContains(fmt.Flags_, FmtFlag::NoDecimalPoint)) {
         if (pout != pstart || IsEnumContains(fmt.Flags_, FmtFlag::ForceDecimalPoint))
            *--pout = NumPunct_Current.DecPoint_;
      }
   }
   else {
      pout = PutAutoPrecision(pstart, value, scale);
      if (pout == pstart && IsEnumContains(fmt.Flags_, FmtFlag::ForceDecimalPoint))
         *--pout = NumPunct_Current.DecPoint_;
   }
   return pout;
}

fon9_API char* DecToStrRev(char* pout, uintmax_t value, bool isNeg, DecScaleT scale, FmtDef fmt) {
   if (fon9_UNLIKELY(value == 0 && IsEnumContains(fmt.Flags_, FmtFlag::Hide0)))
      return ToStrRev_Hide0(pout, fmt);
   char* const pstart = pout;
   pout = ToStrRev_DecScale(pout, value, scale, fmt);
   fmt.Precision_ = 0;
   fmt.Flags_ -= (FmtFlag::Hide0 | FmtFlag::HasPrecision);
   if (fon9_LIKELY(value != 0 || !IsEnumContains(fmt.Flags_, FmtFlag::IntHide0)))
      pout = UIntToStrRev_CheckIntSep(pout, value, fmt.Flags_);
   return IntToStrRev_LastJustify(pout, static_cast<unsigned>(pstart - pout), isNeg, fmt);
}

//--------------------------------------------------------------------------//

fon9_API char* ToStrRev_LastJustify(char* pout, unsigned szout, FmtDef fmt) {
   int szfill = static_cast<int>(fmt.Width_ - szout);
   if (szfill > 0) {
      if (IsEnumContains(fmt.Flags_, FmtFlag::LeftJustify))
         return LeftJustify(pout, szout, static_cast<size_t>(szfill));
      memset(pout -= szfill, ' ', static_cast<size_t>(szfill));
   }
   return pout;
}

fon9_API char* ToStrRev(char* pout, StrView str, FmtDef fmt) {
   if (fon9_UNLIKELY(IsEnumContains(fmt.Flags_, FmtFlag::HasPrecision)))
      str = StrView_TruncUTF8(str, fmt.Precision_);
   size_t strsz = str.size();
   int fsz = static_cast<int>(fmt.Width_ - strsz);
   if (fsz <= 0)
      return reinterpret_cast<char*>(memcpy(pout - strsz, str.begin(), strsz));
   if (IsEnumContains(fmt.Flags_, FmtFlag::LeftJustify)) {
      memset(pout -= fsz, ' ', static_cast<size_t>(fsz));
      memcpy(pout -= str.size(), str.begin(), str.size());
   }
   else {
      memcpy(pout -= str.size(), str.begin(), str.size());
      memset(pout -= fsz, ' ', static_cast<size_t>(fsz));
   }
   return pout;
}

} // namespace fon9
