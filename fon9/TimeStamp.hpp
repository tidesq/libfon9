/// \file fon9/TimeStamp.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_TimeStamp_hpp__
#define __fon9_TimeStamp_hpp__
#include "fon9/TimeInterval.hpp"

namespace fon9 {

/// \ingroup AlNum
/// 用來表達 YYYYMMDDHHMMSS 的整數型別.
using DateTime14T = int64_t;
using DateYYYYMMDD = uint32_t;
using TimeHHMMSS = uint32_t;

/// \ingroup AlNum
/// 時間戳 = seconds since epoch.
/// 小數6位, 可記錄到us (但是否可以精確到us, 則視不同系統而定).
/// 是否為 localtime 或某地時間, 由使用者自行考慮.
class TimeStamp : public TimeInterval {
   using base = TimeInterval;
public:
   using base::base;
   constexpr explicit TimeStamp(const DecimalBase& rhs) : base{rhs} {
   }
   /// 預設值 = 0 = 1970/01/01 00:00:00.
   constexpr TimeStamp() : base{} {
   }
   static constexpr TimeStamp Null() {
      return TimeStamp{DecimalBase::Null()};
   }

   constexpr OrigType ToEpochSeconds() const {
      return this->GetIntPart();
   }
};

/// \ingroup AlNum
/// 取得 UTC 現在時間.
inline TimeStamp UtcNow() {
   using namespace std::chrono;
   static_assert(TimeStamp::Scale == 6, "TimeStamp unit is not microseconds?");
   return TimeStamp{TimeInterval_Microsecond(duration_cast<microseconds>(system_clock::now().time_since_epoch()).count())};
};

//--------------------------------------------------------------------------//

fon9_API const struct tm& EpochSecondsInfo(TimeStamp::OrigType epochSeconds);

/// \ingroup AlNum
/// 拆解 TimeStamp, 使用 struct tm 回覆, 在同一個 thread, 下次呼叫, 傳回值的內容就會被改變.
inline const struct tm& GetDateTimeInfo(TimeStamp ts) {
   return EpochSecondsInfo(ts.ToEpochSeconds());
}

inline void stdtm_AssignYYYYMMDD(struct tm& t, uint32_t yyyymmdd) {
   t.tm_mday = static_cast<int>(yyyymmdd % 100);
   t.tm_mon = static_cast<int>(yyyymmdd / 100) % 100 - 1;
   t.tm_year = static_cast<int>(yyyymmdd / 10000) - 1900;
}
inline void stdtm_AssignYYYYMMDD(struct tm& t, int yyyy, int mm, int dd) {
   t.tm_year = yyyy - 1900;
   t.tm_mon = mm - 1;
   t.tm_mday = dd;
}
inline void stdtm_AssignHHMMSS(struct tm& t, uint32_t hhmmss) {
   t.tm_sec = static_cast<int>(hhmmss % 100);
   t.tm_min = static_cast<int>(hhmmss / 100) % 100;
   t.tm_hour = static_cast<int>(hhmmss / 10000);
}

inline TimeStamp::OrigType stdtm_ToEpochSeconds(const struct tm& t) {
#ifdef fon9_WINDOWS
   return _mkgmtime(const_cast<struct tm*>(&t));
#else
   return timegm(const_cast<struct tm*>(&t));
#endif
}

/// \ingroup AlNum
/// 星期日=0, 星期一=1...
enum class Weekday : uint8_t {
   Sunday = 0,
   Monday = 1,
   Tuesday = 2,
   Wednesday = 3,
   Thursday = 4,
   Friday = 5,
   Saturday = 6,
};
enum {
   kWeekdayCount = 7,
};
/// 星期幾? 0=星期日, 1=星期一...
inline Weekday GetWeekday(TimeStamp ts) {
   return static_cast<Weekday>(GetDateTimeInfo(ts).tm_wday);
}

fon9_API DateTime14T EpochSecondsToYYYYMMDDHHMMSS(TimeStamp::OrigType epochSeconds);

/// \ingroup AlNum
/// 將 TimeStamp 轉成 YYYYMMDDHHMMSS.
inline DateTime14T ToYYYYMMDDHHMMSS(TimeStamp ts) {
   return EpochSecondsToYYYYMMDDHHMMSS(ts.ToEpochSeconds());
}

inline DateYYYYMMDD ToYYYYMMDD(TimeStamp ts) {
   return static_cast<DateYYYYMMDD>(ToYYYYMMDDHHMMSS(ts) / kDivHHMMSS);
}

//--------------------------------------------------------------------------//

/// \ingroup AlNum
/// 將 yyyymmddhhmmss 轉成 epoch seconds.
fon9_API TimeStamp::OrigType YYYYMMDDHHMMSS_ToEpochSeconds(DateTime14T yyyymmddhhmmss);

inline TimeStamp YYYYMMDDHHMMSS_ToTimeStamp(DateTime14T yyyymmddhhmmss) {
   return TimeStamp{TimeInterval_Second(YYYYMMDDHHMMSS_ToEpochSeconds(yyyymmddhhmmss))};
}
inline TimeStamp YYYYMMDDHHMMSS_ToTimeStamp(DateYYYYMMDD yyyymmdd, TimeHHMMSS hhmmss) {
   return YYYYMMDDHHMMSS_ToTimeStamp(yyyymmdd * kDivHHMMSS + hhmmss);
}
inline TimeStamp TimeStampResetHHMMSS(TimeStamp ts, TimeHHMMSS hhmmss = 0) {
   DateTime14T  yyyymmddhhmmss = ToYYYYMMDDHHMMSS(ts);
   return YYYYMMDDHHMMSS_ToTimeStamp(yyyymmddhhmmss - (yyyymmddhhmmss % kDivHHMMSS) + hhmmss);
}

/// \ingroup AlNum
/// 數字轉 epoch seconds.
/// - 若 inum >  9999999999 則表示: inum = yyyymmddHHMMSS, 分解之後轉為 epoch seconds.
/// - 若 inum <= 9999999999 則表示: inum = epoch seconds, 直接返回 inum.
///   用此方法, inum 最多可以表達到 2286/11/20 17:46:39.
inline TimeStamp::OrigType NumToEpochSeconds(TimeStamp::OrigType inum) {
   return(inum <= 9999999999 ? inum : YYYYMMDDHHMMSS_ToEpochSeconds(inum));
}

constexpr TimeStamp EpochSecondsToTimeStamp(TimeStamp::OrigType epochSeconds) {
   return TimeStamp{TimeStamp::Make<0>(epochSeconds)};
}

//--------------------------------------------------------------------------//

/// \ingroup AlNum
/// 字串轉成 TimeStamp.
/// - 主要分成 2 部分: `yyyymmdd HHMMSS`; 中間分隔可用「'-' or SPC」或沒有分隔「yyyyymmddHHMMSS」
/// - yyyymmdd 可用 yyyy/mm/dd 或 yyyy-mm-dd 或 yyyymmdd
/// - HHMMSS 可用 HH:MM:SS 或 HHMMSS 或 MM:SS(此時HH為0);
///   - 若沒提供 HHMMSS 則使用 000000
///   - HHMMSS 之後可包含小數位 `.uuuuuu`
/// - 分隔符號前後允許任意空白.
fon9_API TimeStamp StrTo(const StrView& str, TimeStamp value = TimeStamp::Null(), const char** endptr = nullptr);

enum {
   /// YYYYMMDDHHMMSS.uuuuuu 字串緩衝區大小(不含EOS).
   kDateTimeStrWidth = sizeof("YYYYMMDDHHMMSS.uuuuuu") - 1,
   /// FIX時間格式: YYYYMMDD-HH:MM:SS 字串緩衝區大小(不含EOS).
   kDateTimeStrWidth_FIX = sizeof("YYYYMMDD-HH:MM:SS") - 1,
   /// FIX時間格式(包含ms): YYYYMMDD-HH:MM:SS.sss 字串緩衝區大小(不含OS).
   kDateTimeStrWidth_FIXMS = sizeof("YYYYMMDD-HH:MM:SS.sss") - 1,
};

/// \ingroup AlNum
/// 輸出時間的 YYYYMMDDHHMMSS.uuuuuu 字串(有含EOS).
/// \retval nullptr  無效的 ts
/// \retval !nullptr 時間字串輸出, 在同一個 thread 裡面, 下次呼叫會改變此值.
fon9_API const char* ToStrRev_Full(TimeStamp ts);

/// \ingroup AlNum
/// 輸出時間的 YYYYMMDDHHMMSS.uuuuuu 字串.
inline char* ToStrRev(char* pout, TimeStamp ts) {
   if (const char* pstr = ToStrRev_Full(ts))
      memcpy(pout -= kDateTimeStrWidth, pstr, kDateTimeStrWidth);
   else
      memset(pout -= kDateTimeStrWidth, ' ', kDateTimeStrWidth);
   return pout;
}

/// \ingroup AlNum
/// 輸出時間的 YYYYMMDD-HHMMSS.uuuuuu 字串.
inline char* ToStrRev_Date_Time_us(char* pout, TimeStamp ts) {
   if (const char* pstr = ToStrRev_Full(ts)) {
      memcpy(pout -= 13, pstr + 8, 13);
      *--pout = '-';
      memcpy(pout -= 8, pstr, 8);
      static_assert(kDateTimeStrWidth == 8 + 13, "DateTimeStr is not 'YYYYMMDDHHMMSS.uuuuuu' ?");
   }
   else
      memset(pout -= kDateTimeStrWidth + 1, ' ', kDateTimeStrWidth + 1);
   return pout;
}
template <class RevBuffer>
void RevPut_Date_Time_us(RevBuffer& rbuf, TimeStamp ts) {
   char* pout = rbuf.AllocPrefix(kDateTimeStrWidth + 1);
   pout = ToStrRev_Date_Time_us(pout, ts);
   rbuf.SetPrefixUsed(pout);
}

/// \ingroup AlNum
/// 輸出時間的 YYYYMMDD-HH:MM:SS 字串 (FIX時間格式).
fon9_API char* ToStrRev_FIX(char* pout, TimeStamp ts);

/// \ingroup AlNum
/// 輸出時間的 YYYYMMDD-HH:MM:SS.sss 字串 (FIX時間格式,包含ms).
fon9_API char* ToStrRev_FIXMS(char* pout, TimeStamp ts);

//--------------------------------------------------------------------------//

fon9_WARN_DISABLE_PADDING;
/// \ingroup AlNum
/// 時區調整值.
class TimeZoneOffset {
protected:
   using OffsetMinutesT = int;
private:
   OffsetMinutesT OffsetMinutes_;

   constexpr TimeZoneOffset(OffsetMinutesT ofs) : OffsetMinutes_{ofs} {
   }
public:
   class StrToAux;

   constexpr TimeZoneOffset() : OffsetMinutes_{0} {
   }

   constexpr static TimeZoneOffset FromOffsetMinutes(OffsetMinutesT mins) {
      return TimeZoneOffset{mins};
   }
   constexpr static TimeZoneOffset FromOffsetHHMM(OffsetMinutesT hh, OffsetMinutesT mm) {
      return TimeZoneOffset{hh * 60 + mm};
   }

   TimeInterval ToTimeInterval() const {
      return TimeInterval_Minute(this->OffsetMinutes_);
   }

   inline friend bool operator==(TimeZoneOffset tz1, TimeZoneOffset tz2) {
      return tz1.OffsetMinutes_ == tz2.OffsetMinutes_;
   }
   inline friend bool operator!=(TimeZoneOffset tz1, TimeZoneOffset tz2) {
      return !(tz1 == tz2);
   }

   friend fon9_API char* ToStrRev(char* pout, TimeZoneOffset tzofs);
};
fon9_WARN_POP;

inline TimeStamp operator+(TimeStamp ts, TimeZoneOffset tz) {
   return TimeStamp{ts + tz.ToTimeInterval()};
}
inline TimeStamp& operator+=(TimeStamp& ts, TimeZoneOffset tz) {
   ts += tz.ToTimeInterval();
   return ts;
}

/// \ingroup AlNum
/// 取得系統目前本地時區調整.
/// 在初次使用時設定, 在程式結束前都會取相同的值.
fon9_API TimeZoneOffset GetLocalTimeZoneOffset();

/// \ingroup AlNum
/// 使用 tzName 取得當地時區調整.
/// - tzName == "L": GetLocalTimeZoneOffset();
/// - tzName == "TW": 台灣時間: "+8"
fon9_API TimeZoneOffset GetTimeZoneOffsetByName(StrView tzName);

/// \ingroup AlNum
/// 字串轉 TimeZoneOffset.
/// - "-hh" or "-hhmm" or "-hh:mm";
/// - "+hh" or "+hhmm" or "+hh:mm"; 「+」可省略
/// - "L" retval = 本地調整時間, 前方的「+-」會被忽略.
/// - 'TimeZoneName' 前方的「+-」會被忽略.
///   - 若 TimeZoneName 有空白則必須加上「'」單引號.
///   - 若 TimeZoneName 沒空白則可以不用加上「'」單引號, 用 isspace() 當作 TimeZoneName 的結束.
fon9_API TimeZoneOffset StrTo(const StrView& str, TimeZoneOffset value = TimeZoneOffset{}, const char** endptr = nullptr);

/// \ingroup AlNum
/// 輸出 TimeZoneOffset 時間調整字串: "+h" or "+h:mm" or "-h" or "-h:mm"
fon9_API char* ToStrRev(char* pout, TimeZoneOffset tzofs);
/// 僅支援 fmt.Width_ 及 FmtFlag::LeftJustify.
fon9_API char* ToStrRev(char* pout, TimeZoneOffset tzofs, FmtDef fmt);

//--------------------------------------------------------------------------//

enum class TsFmtItem : char {
   YYYYMMDDHHMMSS,

   YYYYMMDD,
   /// YYYY-MM-DD
   YYYY_MM_DD,
   /// YYYY/MM/DD
   YYYYsMMsDD,

   Year4,
   /// Month as a decimal number (01-12)
   Month02,
   /// Day of the month, zero-padded (01-31)
   Day02,

   // HHMMSS
   HHMMSS,
   /// ISO 8601 time format (HH:MM:SS).
   HH_MM_SS,

   Hour02,
   Minute02,
   Second02,
};

enum class TsFmtItemChar : char {
   YYYYMMDDHHMMSS = 'L',

   YYYYMMDD = 'f',
   YYYY_MM_DD = 'F',
   YYYYsMMsDD = 'K',
   HHMMSS = 't',
   HH_MM_SS = 'T',

   Year4 = 'Y',
   Month02 = 'm',
   Day02 = 'd',

   Hour02 = 'H',
   Minute02 = 'M',
   Second02 = 'S',
};

/// \ingroup AlNum
/// TimeStamp 的格式化定義.
struct fon9_API FmtTS : public FmtDef {
   uint8_t        ItemsCount_{0};
   TsFmtItem      FmtItems_[15];
   TimeZoneOffset TimeZoneOffset_;

   FmtTS() = default;

   /// [TsFmtItemChar[char]][[+-TimeZoneOffset(spc)][width][.precision]]
   /// - TimeZoneOffset 若使用 TimeZoneName 則必須加上「'」單引號，例如： 'TW'
   /// - 如果尾端有 "." 或 ".0" 則自動小數位, 如果尾端沒有 ".precision" 則不顯示小數位.
   FmtTS(StrView fmtstr);
};

template <> struct FmtSelector<TimeStamp> {
   using FmtType = FmtTS;
};

fon9_API char* ToStrRev(char* pout, TimeStamp ts, const FmtTS& fmt);

} // namespaces
#endif//__fon9_TimeStamp_hpp__
