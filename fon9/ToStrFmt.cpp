// \file fon9/ToStrFmt.cpp
// \author fonwinz@gmail.com
#include "fon9/ToStrFmt.hpp"

namespace fon9 {

fon9_API char* ToStrRev_Hide0(char* pout, FmtDef fmt) {
   if (fon9_LIKELY(!IsEnumContainsAny(fmt.Flags_, FmtFlag::MaskSign) || IsEnumContainsAny(fmt.Flags_, FmtFlag::MaskBase))) {
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

template <class FnPrefix>
static char* IntToStrRev_LastJustify(char* pout, char* const pstart, FmtDef fmt, FnPrefix fnPrefix) {
   unsigned szout = static_cast<unsigned>(pstart - pout);
   int szfill = static_cast<int>(fmt.Precision_ - szout);
   if (fon9_UNLIKELY(szfill > 0)) {
      memset(pout -= szfill, '0', static_cast<size_t>(szfill));
      szout += szfill;
   }
   const unsigned szPrefix = fnPrefix.PrefixSize();
   szfill = static_cast<int>(fmt.Width_ - szout - szPrefix);
   if (szfill <= 0) {
      if (szPrefix)
         pout = fnPrefix.ShowPrefix(pout);
   }
   else {
      if (IsEnumContains(fmt.Flags_, FmtFlag::LeftJustify)) {
         if (szPrefix) {
            szout += szPrefix;
            pout = fnPrefix.ShowPrefix(pout);
         }
         return LeftJustify(pout, szout, static_cast<size_t>(szfill));
      }
      char chfill = (IsEnumContains(fmt.Flags_, FmtFlag::HasPrecision) ? ' '
                     : IsEnumContains(fmt.Flags_, FmtFlag::IntPad0) ? '0'
                     : ' ');
      if (chfill == '0' || szPrefix == 0) {
         memset(pout -= szfill, chfill, static_cast<size_t>(szfill));
         if (szPrefix)
            pout = fnPrefix.ShowPrefix(pout);
      }
      else {
         pout = fnPrefix.ShowPrefix(pout);
         memset(pout -= szfill, chfill, static_cast<size_t>(szfill));
      }
   }
   return pout;
}

fon9_API char* IntToStrRev_LastJustify(char* pout, char* const pstart, bool isNeg, FmtDef fmt) {
   struct FnPrefix {
      char SignChar_;
      FnPrefix(bool isNeg, FmtFlag fmtflags) {
         if (isNeg)
            this->SignChar_ = '-';
         else {
            fon9_WARN_DISABLE_SWITCH;
            switch (fmtflags & FmtFlag::MaskSign) {
            case FmtFlag::ShowPlus0: this->SignChar_ = '+';    break;
            case FmtFlag::ShowSign:  this->SignChar_ = ' ';    break;
            default:                 this->SignChar_ = '\0';   break;
            }
            fon9_WARN_POP;
         }
      }
      unsigned PrefixSize() const {
         return static_cast<unsigned>(this->SignChar_ != '\0');
      }
      char* ShowPrefix(char* pout) const {
         *--pout = this->SignChar_;
         return pout;
      }
   };
   return IntToStrRev_LastJustify(pout, pstart, fmt, FnPrefix{isNeg, fmt.Flags_});
}

static char* IntToStrRev_Base(char* pout, uintmax_t value, FmtDef fmt) {
   struct FnPrefix {
      FmtFlag  BaseFlag_;
      unsigned PrefixSize_;
      unsigned PrefixSize() const {
         return this->PrefixSize_;
      }
      char* ShowPrefix(char* pout) const {
         fon9_WARN_DISABLE_SWITCH;
         switch (this->BaseFlag_) {
         case FmtFlag::BaseHex: *--pout = 'x'; *--pout = '0'; break;
         case FmtFlag::BaseHEX: *--pout = 'X'; *--pout = '0'; break;
         case FmtFlag::BaseOct: *--pout = '0'; break;
         case FmtFlag::BaseBin: break;
         }
         fon9_MSC_WARN_POP;
         return pout;
      }
   };
   char* const pstart = pout;
   FnPrefix fnPrefix;
   fon9_WARN_DISABLE_SWITCH;
   switch (fnPrefix.BaseFlag_ = fmt.Flags_ & FmtFlag::MaskBase) {
   default: return nullptr;
   case FmtFlag::BaseHex: fnPrefix.PrefixSize_ = 2; pout = HexToStrRev(pout, value); break;
   case FmtFlag::BaseHEX: fnPrefix.PrefixSize_ = 2; pout = HEXToStrRev(pout, value); break;
   case FmtFlag::BaseOct: fnPrefix.PrefixSize_ = 1; pout = OctToStrRev(pout, value); break;
   case FmtFlag::BaseBin: fnPrefix.PrefixSize_ = 0; pout = BinToStrRev(pout, value); break;
   }
   fon9_MSC_WARN_POP;
   if (value == 0 || !IsEnumContains(fmt.Flags_, FmtFlag::ShowPrefix))
      fnPrefix.PrefixSize_ = 0;
   return IntToStrRev_LastJustify(pout, pstart, fmt, fnPrefix);
}

fon9_API char* IntToStrRev(char* pout, uintmax_t value, bool isNeg, FmtDef fmt) {
   if (fon9_UNLIKELY(value == 0)) {
      if (IsEnumContainsAny(fmt.Flags_, FmtFlag::Hide0)
          || (fmt.Flags_ & (FmtFlag::IntHide0 | FmtFlag::IntPad0)) == FmtFlag::IntHide0
          || (IsEnumContains(fmt.Flags_, FmtFlag::HasPrecision) && fmt.Precision_ == 0)) {
         return ToStrRev_Hide0(pout, fmt);
      }
   }
   if (fon9_UNLIKELY(IsEnumContainsAny(fmt.Flags_, FmtFlag::MaskBase))) {
      if (char* res = IntToStrRev_Base(pout, isNeg ? static_cast<uintmax_t>(-static_cast<intmax_t>(value)) : value, fmt))
         return res;
   }
   char* const pstart = pout;
   pout = UIntToStrRev_CheckIntSep(pstart, value, fmt.Flags_);
   return IntToStrRev_LastJustify(pout, pstart, isNeg, fmt);
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
   return IntToStrRev_LastJustify(pout, pstart, isNeg, fmt);
}

//--------------------------------------------------------------------------//

fon9_API char* ToStrRev_LastJustify(char* pout, char* const pstart, FmtDef fmt) {
   unsigned szout = static_cast<unsigned>(pstart - pout);
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
