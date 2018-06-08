// \file fon9/TimeStamp.cpp
// \author fonwinz@gmail.com
#include "fon9/TimeStamp.hpp"

#ifdef fon9_WINDOWS
inline struct tm* gmtime_r(time_t* secs, struct tm* tm) {
   return (gmtime_s(tm, secs) == 0) ? tm : nullptr;
}
#else
/*
static time_t _mkgmtime(const struct tm* stm) {
   static const int cumdays[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
   long     year = 1900 + stm->tm_year + stm->tm_mon / 12;
   time_t   result = (year - 1970) * 365 + cumdays[stm->tm_mon % 12];
   result += (year - 1968) / 4;
   result -= (year - 1900) / 100;
   result += (year - 1600) / 400;
   if ( (year % 4) == 0
   &&  ((year % 100) != 0 || (year % 400) == 0)
   &&  (stm->tm_mon % 12) < 2 )
      --result;
   result += stm->tm_mday - 1;
   result *= 24;
   result += stm->tm_hour;
   result *= 60;
   result += stm->tm_min;
   result *= 60;
   result += stm->tm_sec;
   //if (stm->tm_isdst == 1)
   //   result -= 3600;
   return (result);
}
*/
#endif

namespace fon9 {

using TsOrigType = TimeStamp::OrigType;
using TimeUS = uint32_t;

fon9_WARN_DISABLE_PADDING;
struct TimeStampThreadLocalCache {
   struct tm   Tm_;
   DateTime14T YYYYMMDDHHMMSS_;
   TsOrigType  EpochSeconds_;
   TimeUS      PartUS_{DecDivisor<TimeUS,6>::Divisor};
   TimeUS      PartFIXMS_{DecDivisor<TimeUS,3>::Divisor};
   char        BufferDateTimeStr_[kDateTimeStrWidth + 1];
   char        BufferDateTimeStr_FIXMS_[kDateTimeStrWidth_FIXMS + 1];

   TimeStampThreadLocalCache() {
      memset(this, 0, sizeof(*this));
      this->BufferDateTimeStr_[14] = '.';
      // YYYYMMDDHHMMSS.uuuuuu
      // 0123456789-123456789-
      // YYYYMMDD-HH:MM:SS.sss
      this->BufferDateTimeStr_FIXMS_[8] = '-';
      this->BufferDateTimeStr_FIXMS_[11] = this->BufferDateTimeStr_FIXMS_[14] = ':';
      this->BufferDateTimeStr_FIXMS_[17] = '.';
   }

   bool CheckCachedTm(TsOrigType epochSeconds) {
      if (this->EpochSeconds_ == epochSeconds)
         return false;
      this->EpochSeconds_ = epochSeconds;
      if (struct tm* tm = gmtime_r(&epochSeconds, &this->Tm_)) {
         this->YYYYMMDDHHMMSS_ = (tm->tm_year + 1900) * 10000000000LL
            + (tm->tm_mon + 1) * 100000000
            + (tm->tm_mday) * 1000000
            + (tm->tm_hour) * 10000
            + (tm->tm_min) * 100
            + (tm->tm_sec);
      }
      else {
         ZeroStruct(this->Tm_);
         this->YYYYMMDDHHMMSS_ = 0;
      }
      this->BufferDateTimeStr_[0] = this->BufferDateTimeStr_FIXMS_[0] = 0;
      return true;
   }
   void ToStr_FIX(TsOrigType epochSeconds) {
      if (this->CheckCachedTm(epochSeconds) || this->BufferDateTimeStr_FIXMS_[0] == 0) {
         Pic9ToStrRev<8>(this->BufferDateTimeStr_FIXMS_ + 8, static_cast<unsigned>(this->YYYYMMDDHHMMSS_ / kDivHHMMSS));
         Put2Digs(this->BufferDateTimeStr_FIXMS_ + 11, static_cast<uint8_t>(this->Tm_.tm_hour));
         Put2Digs(this->BufferDateTimeStr_FIXMS_ + 14, static_cast<uint8_t>(this->Tm_.tm_min));
         Put2Digs(this->BufferDateTimeStr_FIXMS_ + 17, static_cast<uint8_t>(this->Tm_.tm_sec));
      }
   }
   char* ToStrRev_FIX(char* pout, TsOrigType epochSeconds) {
      this->ToStr_FIX(epochSeconds);
      memcpy(pout -= kDateTimeStrWidth_FIX, this->BufferDateTimeStr_FIXMS_, kDateTimeStrWidth_FIX);
      return pout;
   }
   char* ToStrRev_FIXMS(char* pout, TsOrigType epochSeconds, TimeUS ms) {
      this->ToStr_FIX(epochSeconds);
      if (this->PartFIXMS_ != ms) {
         this->PartFIXMS_ = ms;
         Pic9ToStrRev<3>(this->BufferDateTimeStr_FIXMS_ + kDateTimeStrWidth_FIXMS, ms);
      }
      memcpy(pout -= kDateTimeStrWidth_FIXMS, this->BufferDateTimeStr_FIXMS_, kDateTimeStrWidth_FIXMS);
      return pout;
   }
};
fon9_WARN_POP;

static thread_local TimeStampThreadLocalCache  Cached_;

//--------------------------------------------------------------------------//

fon9_API const struct tm& EpochSecondsInfo(TimeStamp::OrigType epochSeconds) {
   Cached_.CheckCachedTm(epochSeconds);
   return Cached_.Tm_;
}

fon9_API DateTime14T EpochSecondsToYYYYMMDDHHMMSS(TimeStamp::OrigType epochSeconds) {
   Cached_.CheckCachedTm(epochSeconds);
   return Cached_.YYYYMMDDHHMMSS_;
}

//--------------------------------------------------------------------------//

fon9_API TsOrigType YYYYMMDDHHMMSS_ToEpochSeconds(DateTime14T yyyymmddhhmmss) {
   struct tm  tm;
   tm.tm_sec = static_cast<int>(yyyymmddhhmmss % 100);
   yyyymmddhhmmss /= 100;
   tm.tm_min = static_cast<int>(yyyymmddhhmmss % 100);
   yyyymmddhhmmss /= 100;
   tm.tm_hour = static_cast<int>(yyyymmddhhmmss % 100);
   yyyymmddhhmmss /= 100;
   tm.tm_mday = static_cast<int>(yyyymmddhhmmss % 100);
   yyyymmddhhmmss /= 100;
   tm.tm_mon = static_cast<int>(yyyymmddhhmmss % 100) - 1;
   tm.tm_year = static_cast<int>(yyyymmddhhmmss / 100) - 1900;
   return stdtm_ToEpochSeconds(tm);
}

fon9_API const char* ToStrRev_Full(TimeStamp ts) {
   if (fon9_LIKELY(!ts.IsNull())) {
      const TsOrigType  es = ts.GetOrigValue();
      const TimeUS      us = static_cast<TimeUS>((es < 0 ? -es : es) % ts.Divisor);
      if (Cached_.CheckCachedTm(ts.ToEpochSeconds()) || Cached_.BufferDateTimeStr_[0] == 0) {
         SPic9ToStrRev<14>(Cached_.BufferDateTimeStr_ + 14, Cached_.YYYYMMDDHHMMSS_);
         goto __SET_US;
      }
      if (Cached_.PartUS_ != us) {
      __SET_US:
         Cached_.PartUS_ = us;
         Pic9ToStrRev<TimeStamp::Scale>(Cached_.BufferDateTimeStr_ + kDateTimeStrWidth, us);
      }
      return Cached_.BufferDateTimeStr_;
   }
   return nullptr;
}

fon9_API char* ToStrRev_FIX(char* pout, TimeStamp ts) {
   if (fon9_LIKELY(!ts.IsNull()))
      return Cached_.ToStrRev_FIX(pout, ts.ToEpochSeconds());
   memset(pout -= kDateTimeStrWidth_FIX, ' ', kDateTimeStrWidth_FIX);
   return pout;
}
fon9_API char* ToStrRev_FIXMS(char* pout, TimeStamp ts) {
   if (fon9_LIKELY(!ts.IsNull())) {
      const TsOrigType  es = ts.GetOrigValue();
      const TimeUS      ms = static_cast<TimeUS>((es < 0 ? -es : es) % ts.Divisor) / 1000;
      return Cached_.ToStrRev_FIXMS(pout, ts.ToEpochSeconds(), ms);
   }
   memset(pout -= kDateTimeStrWidth_FIXMS, ' ', kDateTimeStrWidth_FIXMS);
   return pout;
}

//--------------------------------------------------------------------------//

FmtTS::FmtTS(StrView fmtstr) {
   TsFmtItem item;
   for (const char& chItem : fmtstr) {
      fon9_WARN_DISABLE_SWITCH;
      fon9_MSC_WARN_DISABLE_NO_PUSH(4063);//C4063: case '43' is not a valid value for switch of enum 'fon9::TsFmtItemChar'
      switch (static_cast<TsFmtItemChar>(chItem)) {
      #define case_FmtChar_FmtFlag(itemName) \
         case TsFmtItemChar::itemName: item = TsFmtItem::itemName; break;
      case_FmtChar_FmtFlag(YYYYMMDDHHMMSS);
      case_FmtChar_FmtFlag(YYYYMMDD);
      case_FmtChar_FmtFlag(YYYY_MM_DD);
      case_FmtChar_FmtFlag(YYYYsMMsDD);
      case_FmtChar_FmtFlag(Year4);
      case_FmtChar_FmtFlag(Month02);
      case_FmtChar_FmtFlag(Day02);
      case_FmtChar_FmtFlag(HHMMSS);
      case_FmtChar_FmtFlag(HH_MM_SS);
      case_FmtChar_FmtFlag(Hour02);
      case_FmtChar_FmtFlag(Minute02);
      case_FmtChar_FmtFlag(Second02);
      default:
         if (static_cast<char>(chItem) == '.' || isdigit(static_cast<unsigned char>(chItem))) {
            this->ParseWidth(&chItem, fmtstr.end());
            return;
         }
         item = static_cast<TsFmtItem>(chItem);
         break;
      case static_cast<TsFmtItemChar>('+') :
      case static_cast<TsFmtItemChar>('-') :
         const char* pcur = &chItem + 1;
         if (pcur != fmtstr.end() && (*pcur=='\'' || isdigit(static_cast<unsigned char>(*pcur)))) {
            this->TimeZoneOffset_ = StrTo(StrView{&chItem, fmtstr.end()}, this->TimeZoneOffset_, &pcur);
            if ((pcur = StrFindIf(pcur, fmtstr.end(), isnotspace)) != nullptr)
               this->ParseWidth(pcur, fmtstr.end());
            return;
         }
         item = static_cast<TsFmtItem>(chItem);
         break;
      }
      fon9_WARN_POP;

      if (this->ItemsCount_ >= numofele(this->FmtItems_))
         return;
      this->FmtItems_[this->ItemsCount_++] = item;
   }
}

fon9_API char* ToStrRev(char* const pstart, TimeStamp ts, const FmtTS& fmt) {
   uintmax_t value = abs_cast(ts.GetOrigValue());
   if (ts.IsNull() || (value == 0 && IsEnumContains(fmt.Flags_, FmtFlag::Hide0))) {
      if (fmt.Width_ > 0)
         return reinterpret_cast<char*>(memset(pstart - fmt.Width_, ' ', fmt.Width_));
      return pstart;
   }

   value += fmt.TimeZoneOffset_.ToTimeInterval().GetOrigValue();
   char*       pout = ToStrRev_TimeIntervalDec(pstart, value, fmt);
   DateTime14T yyyymmddHHMMSS = EpochSecondsToYYYYMMDDHHMMSS(static_cast<intmax_t>(value));
   uint8_t     itemIndex = fmt.ItemsCount_;
   if (itemIndex == 0)
      pout = Pic9ToStrRev<14>(pout, abs_cast(yyyymmddHHMMSS));
   else {
      NumOutBuf   nbuf;
      char*       pstrYMDHMS = Pic9ToStrRev<14>(nbuf.end(), abs_cast(yyyymmddHHMMSS));
      for (;;) {
         switch (TsFmtItem item = fmt.FmtItems_[--itemIndex]) {
         case TsFmtItem::YYYYMMDDHHMMSS:
            memcpy(pout -= 14, pstrYMDHMS, 14);
            break;
         case TsFmtItem::YYYYMMDD:
            memcpy(pout -= 8, pstrYMDHMS, 8);
            break;
         case TsFmtItem::YYYY_MM_DD:
            memcpy(pout -= 2, pstrYMDHMS + 6, 2);
            *--pout = '-';
            memcpy(pout -= 2, pstrYMDHMS + 4, 2);
            *--pout = '-';
            memcpy(pout -= 4, pstrYMDHMS, 4);
            break;
         case TsFmtItem::YYYYsMMsDD:
            memcpy(pout -= 2, pstrYMDHMS + 6, 2);
            *--pout = '/';
            memcpy(pout -= 2, pstrYMDHMS + 4, 2);
            *--pout = '/';
            memcpy(pout -= 4, pstrYMDHMS, 4);
            break;
         case TsFmtItem::Year4:
            memcpy(pout -= 4, pstrYMDHMS, 4);
            break;
         case TsFmtItem::Month02:
            memcpy(pout -= 2, pstrYMDHMS + 4, 2);
            break;
         case TsFmtItem::Day02:
            memcpy(pout -= 2, pstrYMDHMS + 6, 2);
            break;
         case TsFmtItem::HHMMSS:
            memcpy(pout -= 6, pstrYMDHMS + 8, 6);
            break;
         case TsFmtItem::HH_MM_SS:
            memcpy(pout -= 2, pstrYMDHMS + 12, 2);
            *--pout = ':';
            memcpy(pout -= 2, pstrYMDHMS + 10, 2);
            *--pout = ':';
            memcpy(pout -= 2, pstrYMDHMS + 8, 2);
            break;
         case TsFmtItem::Hour02:
            memcpy(pout -= 2, pstrYMDHMS + 8, 2);
            break;
         case TsFmtItem::Minute02:
            memcpy(pout -= 2, pstrYMDHMS + 10, 2);
            break;
         case TsFmtItem::Second02:
            memcpy(pout -= 2, pstrYMDHMS + 12, 2);
            break;
         default:
            *--pout = static_cast<char>(item);
            break;
         }
         if (itemIndex <= 0)
            break;
         if (static_cast<size_t>(pstart - pout) >= sizeof(NumOutBuf) - 14) // 避免 buffer overflow!
            break;
      }
   }
   return ToStrRev_LastJustify(pout, pstart, fmt);
}

//--------------------------------------------------------------------------//

fon9_WARN_DISABLE_PADDING;
class StrToTimeStampAux : public StrToDecimalAux<TimeStamp> {
   fon9_NON_COPY_NON_MOVE(StrToTimeStampAux);
   typedef StrToDecimalAux<TimeStamp> base;
   bool HasSplitter_;
   bool CheckValidChar(TempResultT& res, StrToNumCursor& cur);
   TempResultT ResultYYYYMMDD_ToEpochSeconds(TempResultT res);
public:
   StrToTimeStampAux() = default;

   using ResultT = TimeStamp;

   bool CheckSign(char ch1st) {
      this->HasSplitter_ = false;
      return base::CheckSign(ch1st);
   }

   bool IsValidChar(TempResultT& res, StrToNumCursor& cur);

   /// 將 res 整數部分的 yyyymmddHHMMSS 轉成 TimeStamp:
   ResultT MakeResult(TempResultT res, StrToNumCursor& cur);
};
fon9_WARN_POP;

using TsTempResultT = StrToTimeStampAux::TempResultT;

static const char* ParseUInt(const char* pcur, const char* pend, uintmax_t& v) {
   if ((pcur = StrFindIf(pcur, pend, isnotspace)) == nullptr)
      return nullptr;
   if (!isdigit(static_cast<unsigned char>(*pcur)))
      return nullptr;
   v = NaiveStrToUInt(pcur, pend, &pcur);
   return pcur;
}
static const char* ParseUInt100(const char* pcur, const char* pend, uintmax_t& res) {
   uintmax_t v;
   if ((pcur = ParseUInt(pcur, pend, v)) == nullptr)
      return nullptr;
   res = (res * 100) + v;
   return pcur;
}
static const char* ParseUInt100(const char* pcur, const char* pend, uintmax_t min, uintmax_t max, TsTempResultT& res) {
   uintmax_t v;
   if ((pcur = ParseUInt(pcur, pend, v)) == nullptr)
      return nullptr;
   if (v < min || max < v)
      return nullptr;
   res = (res * 100) + v;
   return pcur;
}
static bool CheckDecPoint(StrToTimeStampAux& aux, StrToNumCursor& cur) {
   if (*cur.Current_ == '.' || *cur.Current_ == NumPunct_Current.DecPoint_) {
      aux.SetPointPos(cur);
      return true;
   }
   return false;
}

// res = yyyymmdd;  hms = HH or MM;  cur = "MM:SS" or "SS"
static bool ParseMMSS(StrToTimeStampAux& aux, StrToNumCursor& cur, uintmax_t hms, TsTempResultT& res) {
   if (const char* pcur = ParseUInt100(cur.Current_, cur.ExpectEnd_, hms)) {
      if ((pcur = StrFindIf(cur.Current_ = pcur, cur.ExpectEnd_, isnotspace)) == nullptr) {
__RETURN_CURRENT_IsInvalid:
         res = res * kDivHHMMSS + hms;
         return false;
      }
      if (*pcur == ':') {
         if ((pcur = ParseUInt100(cur.Current_ = pcur + 1, cur.ExpectEnd_, hms)) == nullptr)
            goto __FORMAT_ERROR;
         if ((pcur = StrFindIf(cur.Current_ = pcur, cur.ExpectEnd_, isnotspace)) == nullptr)
            goto __RETURN_CURRENT_IsInvalid;
      }
      cur.Current_ = pcur;
      res = res * kDivHHMMSS + hms;
      return CheckDecPoint(aux, cur);
   }
__FORMAT_ERROR:
   cur.FormatErrorAtCurrent();
   return false;
}
// res = yyyymmdd;  pcur = "HHMMSS" or "HH:MM:SS"
static bool ParseHHMMSS(StrToTimeStampAux& aux, const char* pcur, StrToNumCursor& cur, TsTempResultT& res) {
   uintmax_t hms = NaiveStrToUInt(pcur, cur.ExpectEnd_, &pcur);
   if ((pcur = StrFindIf(cur.Current_ = pcur, cur.ExpectEnd_, isnotspace)) == nullptr) {
      res = res * kDivHHMMSS + hms;
      return false;
   }
   if (*pcur == ':') {
      cur.Current_ = pcur + 1;
      return ParseMMSS(aux, cur, hms, res);
   }
   res = res * kDivHHMMSS + hms;
   return CheckDecPoint(aux, cur);
}
// res = yyyymmdd;  cur = "-HHMMSS" or "  HHMMSS" or ""
static bool CheckParseHHMMSS(StrToTimeStampAux& aux, StrToNumCursor& cur, TsTempResultT& res) {
   if (const char* pcur = StrFindIf(cur.Current_, cur.ExpectEnd_, isnotspace)) {
      if (*pcur == '-') { // pcur: "-HHMMSS"
         if ((pcur = StrFindIf(cur.Current_ = pcur + 1, cur.ExpectEnd_, isnotspace)) == nullptr
             || !isdigit(static_cast<unsigned char>(*pcur))) {
            cur.FormatErrorAtCurrent();
            return false;
         }
         return ParseHHMMSS(aux, pcur, cur, res);
      }
      if (isdigit(static_cast<unsigned char>(*pcur)))
         return ParseHHMMSS(aux, pcur, cur, res);
      cur.Current_ = pcur;
   }
   res *= kDivHHMMSS;
   return false;
}
bool StrToTimeStampAux::CheckValidChar(TempResultT& res, StrToNumCursor& cur) {
   if (base::IsValidChar(res, cur))
      return true;
   if (this->HasSplitter_)
      return false;
   const char* pcur = StrFindIf(cur.Current_, cur.ExpectEnd_, isnotspace);
   if (pcur == nullptr)
      return false;

   this->HasSplitter_ = true;

   switch (*pcur) {
   default:
      if (isdigit(static_cast<unsigned char>(*pcur)))
         return ParseHHMMSS(*this, pcur, cur, res);
      // "nnnnn  .uuuu" or "nnnn  XXXX" 此時的 "." or "X" 為無效字元!
      if (res < DecDivisor<TempResultT, 8>::Divisor)// res is "yyyymmdd"
         res *= kDivHHMMSS;
      return false;
   case '/': // "yyyy/mm/dd..."
      if ((pcur = ParseUInt100(cur.Current_ = pcur + 1, cur.ExpectEnd_, 1, 12, res)) == nullptr
          || (pcur = StrFindIf(cur.Current_ = pcur, cur.ExpectEnd_, isnotspace)) == nullptr
          || (*pcur != '/')
          || (pcur = ParseUInt100(cur.Current_ = pcur + 1, cur.ExpectEnd_, 1, 31, res)) == nullptr) {
__FORMAT_ERROR:
         cur.FormatErrorAtCurrent();
         return false;
      }
      cur.Current_ = pcur;
      return CheckParseHHMMSS(*this, cur, res);
   case '-': // "yyyy-mm-dd..." or "yyyymmdd-HHMMSS" or "yyyymmdd-HH:MM:SS"
      uintmax_t hms;
      const char* pAfterFirstSplitter = pcur + 1;
      if ((pcur = ParseUInt(cur.Current_ = pAfterFirstSplitter, cur.ExpectEnd_, hms)) == nullptr)
         goto __FORMAT_ERROR;
      if ((pcur = StrFindIf(cur.Current_ = pcur, cur.ExpectEnd_, isnotspace)) == nullptr) {
         // "res-hms  " => "yyyymmdd-HHMMSS  "
         res = res * kDivHHMMSS + hms;
         return false;
      }
      switch (*pcur) {
      default:
         res = res * kDivHHMMSS + hms;
         cur.Current_ = pcur;
         return CheckDecPoint(*this, cur);
      case '-': // "res-hms-pcur" => "yyyy-mm-dd"
         if (hms < 1 || 12 < hms) {
            cur.Current_ = pAfterFirstSplitter;
            goto __FORMAT_ERROR;
         }
         res = res * 100 + hms;
         if ((pcur = ParseUInt100(cur.Current_ = pcur + 1, cur.ExpectEnd_, 1, 31, res)) == nullptr)
            goto __FORMAT_ERROR;
         cur.Current_ = pcur;
         return CheckParseHHMMSS(*this, cur, res);
      case ':': // "res - hms:pcur" = "yyyymmdd - HH:MM..."
         cur.Current_ = pcur + 1;
         return ParseMMSS(*this, cur, hms, res);
      }
      goto __FORMAT_ERROR;
   }
}
StrToTimeStampAux::TempResultT StrToTimeStampAux::ResultYYYYMMDD_ToEpochSeconds(TempResultT res) {
   TsOrigType epochSeconds = YYYYMMDDHHMMSS_ToEpochSeconds(this->Negative_ == IsPositive
                                                           ? static_cast<DateTime14T>(res)
                                                           : -static_cast<DateTime14T>(res));
   if (epochSeconds >= 0)
      return unsigned_cast(epochSeconds);
   this->Negative_ = IsNegative;
   return unsigned_cast(-epochSeconds);
}
bool StrToTimeStampAux::IsValidChar(TempResultT& res, StrToNumCursor& cur) {
   const bool bfNoPoint = (this->PointPos_ == nullptr);
   const bool retval = this->CheckValidChar(res, cur);
   if ((this->PointPos_ != nullptr) && bfNoPoint) {
      // 必須在首次遇到小數點時, 將 yyyymmddHHMMSS => epoch seconds,
      // 否則 TempResultT 儲存 yyyymmddHHMMSS.uuuuuu 可能會 overflow!
      res = this->ResultYYYYMMDD_ToEpochSeconds(res);
   }
   return retval;
}
StrToTimeStampAux::ResultT StrToTimeStampAux::MakeResult(TempResultT res, StrToNumCursor& cur) {
   if (this->PointPos_ == nullptr)
      res = this->ResultYYYYMMDD_ToEpochSeconds(res);
   return base::MakeResult(res, cur);
}

fon9_API TimeStamp StrTo(StrView str, TimeStamp value, const char** endptr) {
   return StrTo<TimeStamp, StrToTimeStampAux>(str, value, endptr);
}

//--------------------------------------------------------------------------//

fon9_API TimeZoneOffset GetLocalTimeZoneOffset() {
   struct LocalTimeZoneOffset : public TimeZoneOffset {
      LocalTimeZoneOffset() : TimeZoneOffset{GetLocal()} {
      }
      static TimeZoneOffset GetLocal() {
         time_t      tt = 24 * 60 * 60 * 300;
         struct tm   tm;
      #ifdef _MSC_VER
         localtime_s(&tm, &tt);
      #else
         localtime_r(&tt, &tm);
      #endif
         tm.tm_isdst = -1;
         return FromOffsetMinutes(static_cast<OffsetMinutesT>((stdtm_ToEpochSeconds(tm) - tt) / 60));
      }
   };
   static LocalTimeZoneOffset LocalTimeOffsetMinutes_;
   return LocalTimeOffsetMinutes_;
}

fon9_API TimeZoneOffset GetTimeZoneOffsetByName(StrView tzName) {
   if (tzName == "TW") // Taiwan: +8
      return TimeZoneOffset::FromOffsetMinutes(8 * 60);
   if (tzName == "L") // Local.
      return GetLocalTimeZoneOffset();
   // TODO: tzName 使用文字表示法, 例: Linux:'Asia/Taipei'?  Windows:'Taipei Standard Time'?
   // 或是自建資料表: 如此才能跨平台(Linux, Windows)?
   return TimeZoneOffset{};
}

fon9_API char* ToStrRev(char* pout, TimeZoneOffset tzofs) {
   TimeZoneOffset::OffsetMinutesT ofs = tzofs.OffsetMinutes_;
   const bool isNeg = (ofs < 0);
   if (isNeg)
      ofs = -ofs;
   if (uint8_t min = static_cast<uint8_t>(ofs % 60)) {
      pout = Put2Digs(pout, min);
      *--pout = ':';
   }
   pout = ToStrRev(pout, unsigned_cast(ofs / 60));
   *--pout = (isNeg ? '-' : '+');
   return pout;
}

fon9_API char* ToStrRev(char* pstart, TimeZoneOffset tzofs, FmtDef fmt) {
   char* pout = ToStrRev(pstart, tzofs);
   return ToStrRev_LastJustify(pout, pstart, fmt);
}

//--------------------------------------------------------------------------//

fon9_WARN_DISABLE_PADDING;
class TimeZoneOffset::StrToAux : public StrToSIntAux<OffsetMinutesT> {
   using base = StrToSIntAux<OffsetMinutesT>;
   bool HasSplitter_;
public:
   using ResultT = TimeZoneOffset;
   bool CheckSign(char ch1st) {
      this->HasSplitter_ = false;
      return base::CheckSign(ch1st);
   }
   bool IsValidChar(TempResultT& res, StrToNumCursor& cur);
   ResultT MakeResult(TempResultT res, StrToNumCursor& cur);
};
fon9_WARN_POP;

bool TimeZoneOffset::StrToAux::IsValidChar(TempResultT& res, StrToNumCursor& cur) {
   this->HasSplitter_ = true;

   if (cur.Current_ == cur.OrigBegin_) { // ("+" or "-") + spaces + TimeZoneName
      size_t  curLeftLength = static_cast<size_t>(cur.ExpectEnd_ - cur.Current_ - 1);
      const char* tzNameEnd;
      bool        isCurrentEndAdd1;
      if (*cur.Current_ == '\'') {
         tzNameEnd = StrView::traits_type::find(++cur.Current_, curLeftLength, '\'');
         isCurrentEndAdd1 = (tzNameEnd != nullptr);
      }
      else {
         isCurrentEndAdd1 = false;
         tzNameEnd = StrFindIf(cur.Current_ + 1, cur.ExpectEnd_, isspace);
      }
      StrView tzName{cur.Current_, (tzNameEnd ? tzNameEnd : cur.ExpectEnd_)};
      cur.Current_ = tzName.end() + isCurrentEndAdd1;

      TimeZoneOffset tz = GetTimeZoneOffsetByName(tzName);
      if (tz.OffsetMinutes_ >= 0) {
         this->Negative_ = IsPositive;
         res = unsigned_cast(tz.OffsetMinutes_);
      }
      else {
         this->Negative_ = IsNegative;
         res = unsigned_cast(-tz.OffsetMinutes_);
      }
      return false;
   }

   if (cur.Current_ - cur.OrigBegin_ >= 3) { // hhmm or hmm
      res = (res / 100) * 60 + (res % 100);
      return false;
   }
   res *= 60;
   if (const char* pend = StrFindIf(cur.Current_, cur.ExpectEnd_, isnotspace)) {
      if (*pend == ':') {
         if ((pend = StrFindIf(pend + 1, cur.ExpectEnd_, isnotspace)) == nullptr
             || !isdigit(static_cast<unsigned char>(*pend))) {
            cur.FormatErrorAtCurrent();
            return false;
         }
         res += static_cast<int>(NaiveStrToUInt(pend, cur.ExpectEnd_, &pend));
         cur.Current_ = pend;
      }
   }
   return false;
}
TimeZoneOffset::StrToAux::ResultT TimeZoneOffset::StrToAux::MakeResult(TempResultT res, StrToNumCursor& cur) {
   if (!this->HasSplitter_) {
      if (fon9_LIKELY(cur.Current_ - cur.OrigBegin_ <= 2)) // hh
         res = res * 60;
      else // hhmm or hmm
         res = (res / 100) * 60 + (res % 100);
   }
   return base::MakeResult(res, cur);
}

fon9_API TimeZoneOffset StrTo(StrView str, TimeZoneOffset value, const char** endptr) {
   return StrTo<TimeZoneOffset, TimeZoneOffset::StrToAux>(str, value, endptr);
}

} // namespaces
