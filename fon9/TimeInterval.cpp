// \file fon9/TimeInterval.cpp
// \author fonwinz@gmail.com
#include "fon9/TimeInterval.hpp"

namespace fon9 {

static char* ToStrRev_TimeInterval_HHMM(char* pout, uint32_t daySeconds) {
   unsigned mm = (static_cast<unsigned>(daySeconds) / 60u) % 60u;
   unsigned hh = (static_cast<unsigned>(daySeconds) / (60u * 60u)) % 24;
   if (hh || mm) {
      if ((static_cast<unsigned>(daySeconds) % 60u) < 10)
         *--pout = '0';
      *--pout = ':';
      pout = Pic9ToStrRev<2>(pout, mm);
      if (hh) {
         *--pout = ':';
         pout = Pic9ToStrRev<2>(pout, hh);
      }
   }
   return pout;
}

fon9_API char* ToStrRev(char* pout, TimeInterval ti) {
   bool isNeg = (ti.GetOrigValue() < 0);
   if (fon9_UNLIKELY(isNeg)) {
      if (ti.IsNull())
         return pout;
      ti.SetOrigValue(-ti.GetOrigValue());
   }

   using TiUnsigned = typename std::make_unsigned<TimeInterval::OrigType>::type;
   TiUnsigned daySeconds = unsigned_cast(ti.GetIntPart());
   if (daySeconds > 0) {
      /// 輸出 TimeInterval 字串, 基本格式為: `days-hh:mm:ss.uuuuuu`; 例: `3-12:34:56.789`
      /// - 若 days==0, 則省略 `days-`; 例: `12:34:56.789`
      /// - 若 hh==0, 則省略 `hh:`; 例: `3-34:56.789`; `34:56.789`
      /// - 若 hh==0 && mm==0, 則省略 `hh:mm:`; 例: `3-56.789`; `56.789`
      ti.SetOrigValue(ti.GetOrigValue() % TimeInterval_Minute(1).GetOrigValue());
      pout = ToStrRev(pout, ti.ToDecimal());

      TiUnsigned days = unsigned_cast(daySeconds / kOneDaySeconds);
      pout = ToStrRev_TimeInterval_HHMM(pout, static_cast<uint32_t>(daySeconds));
      if (days > 0) {
         *--pout = '-';
         pout = UIntToStrRev(pout, days);
      }
   }
   else {
      /// - 若 (days==0 && hh==0 && mm==0 && ss==0) 也就是 (ti < 1 s), 則:
      ///   - ti >= 1 ms: `123.456 ms`
      ///   - ti < 1 ms:  `456 us`
      if (uint32_t us = static_cast<uint32_t>(ti.GetOrigValue())) {
         *--pout = 's';
         if (us >= 1000) { // ms
            *--pout = 'm';
            *--pout = ' ';
            pout = ToStrRev(pout, Decimal<uint32_t, 3>::Make<3>(us));
         }
         else { // us
            *--pout = 'u';
            *--pout = ' ';
            pout = UIntToStrRev(pout, us);
         }
      }
      else
         *--pout = '0';
   }
   if (isNeg)
      *--pout = '-';
   return pout;
}

fon9_API char* ToStrRev_TimeIntervalDec(char* pout, uintmax_t& value, FmtDef fmt) {
   if (fon9_LIKELY(!IsEnumContains(fmt.Flags_, FmtFlag::HasPrecision)))
      value /= TimeInterval::Divisor;
   else {
      if (fon9_LIKELY(fmt.Precision_ == 0))
         pout = PutAutoPrecision(pout, value, TimeInterval::Scale);
      else {
         pout = ToStrRev_DecScalePrecision(pout, value, TimeInterval::Scale, fmt.Precision_, [](uintmax_t val, uintmax_t div) {
            // 無條件捨去.
            return val / div;
         });
         *--pout = NumPunct_Current.DecPoint_;
      }
   }
   return pout;
}

fon9_API char* ToStrRev(char* const pstart, TimeInterval ti, FmtDef fmt) {
   uintmax_t value = abs_cast(ti.GetOrigValue());
   if (ti.IsNull() || (value == 0 && IsEnumContains(fmt.Flags_, FmtFlag::Hide0))) {
      if (fmt.Width_ > 0)
         return reinterpret_cast<char*>(memset(pstart - fmt.Width_, ' ', fmt.Width_));
      return pstart;
   }
   char*    pout = ToStrRev_TimeIntervalDec(pstart, value, fmt);
   unsigned ss = (static_cast<unsigned>(value) % 60u);
   if (ss < 10)
      *--pout = static_cast<char>(ss + '0');
   else
      pout = Pic9ToStrRev<2>(pout, ss);

   uintmax_t   days = unsigned_cast(value / kOneDaySeconds);
   pout = ToStrRev_TimeInterval_HHMM(pout, static_cast<uint32_t>(value));
   fmt.Precision_ = 0;
   fmt.Flags_ -= FmtFlag::Hide0 | FmtFlag::HasPrecision;
   if (days > 0 || IsEnumContains(fmt.Flags_, FmtFlag::IntPad0)) {
      *--pout = '-';
      pout = UIntToStrRev(pout, days);
   }
   return IntToStrRev_LastJustify(pout, pstart, ti.GetOrigValue() < 0, fmt);
}

//--------------------------------------------------------------------------//

fon9_WARN_DISABLE_PADDING;
class StrToTimeIntervalAux : public StrToDecimalAux<TimeInterval> {
   fon9_NON_COPY_NON_MOVE(StrToTimeIntervalAux);
   typedef StrToDecimalAux<TimeInterval> base;
   bool HasSplitter_;
public:
   StrToTimeIntervalAux() = default;

   using ResultT = TimeInterval;

   bool CheckSign(char ch1st) {
      this->HasSplitter_ = false;
      return base::CheckSign(ch1st);
   }

   /// 在首次遇到無效字元時, 就會接手後續的解析工作.
   /// - 支援的格式參考 \ref char* ToStrRev(char* pout, TimeInterval ti);
   /// - 額外支援的格式: `days-hhmmss.uuuuuu` 其中 `days-` 可省略.
   bool IsValidChar(TempResultT& res, StrToNumCursor& cur);

   /// 將整數部分的 dddhhmmss 轉成秒數:
   /// (ddd * 24*60*60) + (hh * 60*60) + (mm * 60) + ss
   ResultT MakeResult(TempResultT res, StrToNumCursor& cur) const;
};
fon9_WARN_POP;

static const char* ParseUInt100(const char* pcur, const char* pend, StrToTimeIntervalAux::TempResultT& res) {
   if ((pcur = StrFindIf(pcur, pend, isnotspace)) == nullptr)
      return nullptr;
   if (!isdigit(static_cast<unsigned char>(*pcur)))
      return nullptr;
   res = (res * 100) + NaiveStrToUInt(pcur, pend, &pcur);
   return pcur;
}
static bool CheckDecPoint(StrToTimeIntervalAux& aux, StrToNumCursor& cur) {
   if (*cur.Current_ == '.' || *cur.Current_ == NumPunct_Current.DecPoint_) {
      aux.SetPointPos(cur);
      return true;
   }
   return false;
}
// res = "nn:"; 解析後續內容: "ss" or "ss.uuuuuu" or "mm:ss" or "mm:ss.uuuuuu"
static bool ParseMMSS(StrToTimeIntervalAux& aux, StrToTimeIntervalAux::TempResultT days, StrToTimeIntervalAux::TempResultT& res, StrToNumCursor& cur) {
   if (const char* pcur = ParseUInt100(cur.Current_, cur.ExpectEnd_, res)) {
      if ((pcur = StrFindIf(cur.Current_ = pcur, cur.ExpectEnd_, isnotspace)) == nullptr) { // "aa:bb"
__RETURN_CURRENT_IsInvalid:
         res = days * kDivHHMMSS + res;
         return false;
      }
      if (*pcur == ':') { // "aa:bb:cc"
         if ((pcur = ParseUInt100(cur.Current_ = pcur + 1, cur.ExpectEnd_, res)) == nullptr)
            goto __FORMAT_ERROR;
         if ((pcur = StrFindIf(cur.Current_ = pcur, cur.ExpectEnd_, isnotspace)) == nullptr)
            goto __RETURN_CURRENT_IsInvalid;
      }
      // "aa:bb:cc.uuuuuu" or "aa:bb:cc XXXXX"
      cur.Current_ = pcur;
      res = days * kDivHHMMSS + res;
      return CheckDecPoint(aux, cur);
   }
__FORMAT_ERROR:
   cur.FormatErrorAtCurrent();
   return false;
}
// res = "day-"; 解析後續內容: "hhmmss" or "hhmmss.uuuuuu" or "hh:mm:ss" or "hh:mm:ss.uuuuuu" or ...
static bool ParseHHMMSS(StrToTimeIntervalAux& aux, StrToTimeIntervalAux::TempResultT& res, StrToNumCursor& cur) {
   const char* pcur;
   uintmax_t hms = NaiveStrToUInt(cur.Current_, cur.ExpectEnd_, &pcur);
   if ((pcur = StrFindIf(cur.Current_ = pcur, cur.ExpectEnd_, isnotspace)) != nullptr) {
      if (*pcur == ':') {
         StrToTimeIntervalAux::TempResultT days = res;
         res = hms;
         cur.Current_ = pcur + 1;
         return ParseMMSS(aux, days, res, cur);
      }
      cur.Current_ = pcur;
      if (CheckDecPoint(aux, cur)) {
         res = res * kDivHHMMSS + hms;
         return true;
      }
   }
   res = res * kDivHHMMSS + hms;
   return false;
}

bool StrToTimeIntervalAux::IsValidChar(TempResultT& res, StrToNumCursor& cur) {
   if (base::IsValidChar(res, cur))
      return true;
   if (this->HasSplitter_)
      return false;
   const char* pcur = StrFindIf(cur.Current_, cur.ExpectEnd_, isnotspace);
   if (pcur == nullptr)
      return false;
   if (cur.ExpectEnd_ - pcur >= 2 && pcur[1] == 's') {
      switch (pcur[0]) { // ms, us, ns
      case 'm': this->Scale_ += 3;  break;
      case 'u': this->Scale_ += 6;  break;
      case 'n': this->Scale_ += 9;  break;
      default:  return false;
      }
      cur.Current_ = pcur + 2;
      return false;
   }
   if (this->PointPos_) // 小數點之後, 不會有其他格式, 所以必定為無效字元!
      return false;

   this->HasSplitter_ = true;
   switch (*pcur) {
   case '-': // res = "days-"
      if ((pcur = StrFindIf(pcur + 1, cur.ExpectEnd_, isnotspace)) == nullptr)
         return false;
      // 不用 break; 檢查接下來 *pcur 是否為數字:
      // - 不是數字則為無效字元;
      // - 是數字則繼續解析: "hh:mm:ss.uuuuuu" or "mm:ss.uuuuuu" or "ss.uuuuuu";
   default: // res = "days "
      if (!isdigit(static_cast<unsigned char>(*pcur)))
         return false;
      cur.Current_ = pcur;
      return ParseHHMMSS(*this, res, cur);
   case ':': // (res = "hh:" or "mm:") and (no days)
      cur.Current_ = pcur + 1;
      return ParseMMSS(*this, 0, res, cur);
   }
}

StrToTimeIntervalAux::ResultT StrToTimeIntervalAux::MakeResult(TempResultT res, StrToNumCursor& cur) const {
   if (*(cur.Current_ - 1) == 's')
      return base::MakeResult(res, cur);

   TimeInterval::OrigType  retval = base::MakeResult(res, cur).GetOrigValue();
   bool isNeg = (retval < 0);
   if (isNeg)
      retval = -retval;
   const TimeInterval::OrigType dec = (retval % TimeInterval::Divisor);
   retval /= TimeInterval::Divisor;

   const unsigned hhmmss = static_cast<unsigned>(retval % kDivHHMMSS);
   retval = (TimeInterval_Day(retval / kDivHHMMSS)
          + TimeInterval_HHMMSS(hhmmss / 10000u, (hhmmss / 100u) % 100u, (hhmmss % 100u))).GetOrigValue()
          + dec;
   return TimeInterval::Make<TimeInterval::Scale>(isNeg ? -retval : retval);
}

fon9_API TimeInterval StrTo(StrView str, TimeInterval value, const char** endptr) {
   return StrTo<TimeInterval, StrToTimeIntervalAux>(str, value, endptr);
}

//--------------------------------------------------------------------------//

static char* ToStrRevNN(char* pout, DayTimeSec::Value& v) {
   unsigned nn = (v % 60u);
   v /= 60u;
   pout = Pic9ToStrRev<2>(pout, nn);
   *--pout = ':';
   return pout;
}

fon9_API char* ToStrRev(char* pout, DayTimeSec value) {
   pout = ToStrRevNN(pout, value.Seconds_);
   pout = ToStrRevNN(pout, value.Seconds_);
   pout = Pic9ToStrRev<2>(pout, value.Seconds_ % 24);
   if ((value.Seconds_ /= 24) > 0) {
      *--pout = '-';
      pout = UIntToStrRev(pout, value.Seconds_);
   }
   return pout;
}

fon9_API char* ToStrRev(char* const pstart, DayTimeSec value, FmtDef fmt) {
   char* pout = ToStrRev(pstart, value);
   return IntToStrRev_LastJustify(pout, pstart, false, fmt);
}

} // namespaces
