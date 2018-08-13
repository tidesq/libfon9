/// \file fon9/TimeInterval.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_TimeInterval_hpp__
#define __fon9_TimeInterval_hpp__
#include "fon9/Decimal.hpp"

fon9_BEFORE_INCLUDE_STD;
#include <chrono>
fon9_AFTER_INCLUDE_STD;

namespace fon9 {

enum : int64_t {
   kOneDaySeconds = 24 * 60 * 60,
   kDivHHMMSS = 1000000,
};

/// \ingroup AlNum
/// 時間間隔型別: 整數單位=秒, 小數部分可記錄到us.
class TimeInterval : public Decimal<int64_t, 6> {
   using base = Decimal<int64_t, 6>;
protected:
   constexpr explicit TimeInterval(OrigType us) : base(us) {
   }
public:
   using DecimalBase = base;
   using base::base;
   constexpr TimeInterval(const DecimalBase& rhs) : base{rhs} {
   }
   constexpr TimeInterval() : base{} {
   }
   static constexpr TimeInterval Null() {
      return TimeInterval{base::Null()};
   }

   /// 調整時間(秒數): 往未來:secs>0; 往過去:secs<0;
   void AddSeconds(OrigType secs) {
      *this = *this + base::Make<0>(secs);
   }
   TimeInterval& operator=(const DecimalBase& rhs) {
      *static_cast<base*>(this) = rhs;
      return *this;
   }

   using Duration = std::chrono::duration<OrigType, std::ratio<1, Divisor>>;
   constexpr Duration ToDuration() const {
      return Duration{this->GetOrigValue()};
   }
   constexpr DecimalBase ToDecimal() const {
      return *this;
   }
};

constexpr TimeInterval TimeInterval_Microsecond(TimeInterval::OrigType us) { return TimeInterval::Make<6>(us); }
constexpr TimeInterval TimeInterval_Millisecond(TimeInterval::OrigType ms) { return TimeInterval::Make<3>(ms); }
constexpr TimeInterval TimeInterval_Second(TimeInterval::OrigType seconds) { return TimeInterval::Make<0>(seconds); }
constexpr TimeInterval TimeInterval_Minute(TimeInterval::OrigType min) { return TimeInterval_Second(min * 60); }
constexpr TimeInterval TimeInterval_Hour(TimeInterval::OrigType hr) { return TimeInterval_Minute(hr * 60); }
constexpr TimeInterval TimeInterval_Day(TimeInterval::OrigType day) { return TimeInterval_Hour(day * 24); }
constexpr TimeInterval TimeInterval_HHMMSS(unsigned hh, unsigned mm, unsigned ss) {
   return TimeInterval_Second((((static_cast<TimeInterval::OrigType>(hh) * 60) + mm) * 60) + ss);
}

/// \ingroup AlNum
/// 輸出 TimeInterval 字串, 基本格式為: `days-hh:mm:ss.uuuuuu`; 例: `3-12:34:56.789`
/// - 若 days==0, 則省略 `days-`; 例: `12:34:56.789`
/// - 若 hh==0, 則省略 `hh:`; 例: `3-34:56.789`; `34:56.789`
/// - 若 hh==0 && mm==0, 則省略 `hh:mm:`; 例: `3-56.789`; `56.789`
/// - 若 (days==0 && hh==0 && mm==0 && ss==0) 也就是 (ti < 1 s), 則:
///   - ti >= 1 ms: `123.456 ms`
///   - ti < 1 ms:  `456 us`
fon9_API char* ToStrRev(char* pout, TimeInterval ti);

/// \ingroup AlNum
/// - value.IsNull() 則一律填入空白, 由 fmt.Width_ 指定空白數量(可為0).
/// - FmtFlag::IntPad0 會用在 days 上面, 預設: days==0 時, 不顯示 `days-`
/// - fmt.Precision_ 可指定小數位數
/// - 使用 fmt 就不考慮 ms、us 表示法。
fon9_API char* ToStrRev(char* pout, TimeInterval value, FmtDef fmt);

/// 顯示 TimeInterval 的小數部分.
fon9_API char* ToStrRev_TimeIntervalDec(char* pout, uintmax_t& value, FmtDef fmt);

/// \ingroup AlNum
/// 字串轉成 TimeInterval, 支援的格式參考 \ref char* ToStrRev(char* pout, TimeInterval ti);
fon9_API TimeInterval StrTo(StrView str, TimeInterval value = TimeInterval::Null(), const char** endptr = nullptr);

//--------------------------------------------------------------------------//

/// \ingroup AlNum
/// 一天內的時間(秒數), 例: 0 = 00:00:00; 86399 = 23:59:59
struct DayTimeSec : public Comparable<DayTimeSec> {
   using Value = uint32_t;
   Value Seconds_;
   constexpr DayTimeSec() : Seconds_{0} {}
   explicit constexpr DayTimeSec(Value v) : Seconds_{v} {}
   constexpr DayTimeSec(uint32_t h, uint32_t m, uint32_t s)
      : Seconds_{((h * 60) + m) * 60 + s} {
   }
   unsigned ToHHMMSS() const {
      return (this->Seconds_ % 60) + ((this->Seconds_ / 60) % 60) * 100 + (this->Seconds_ / (60 * 60)) * 10000;
   }
   bool operator==(DayTimeSec r) const { return this->Seconds_ == r.Seconds_; }
   bool operator<(DayTimeSec r) const { return this->Seconds_ < r.Seconds_; }
};

/// \ingroup AlNum
/// 支援的格式參考 \ref char* ToStrRev(char* pout, TimeInterval ti);
inline DayTimeSec StrTo(const StrView& str, DayTimeSec nullValue, const char** endptr = nullptr) {
   TimeInterval ti = StrTo(str, TimeInterval::Null(), endptr);
   return ti.IsNull() ? nullValue : DayTimeSec{static_cast<DayTimeSec::Value>(ti.GetIntPart())};
}

inline unsigned StrToHHMMSS(const StrView& str) {
   return StrTo(str, DayTimeSec{}).ToHHMMSS();
}

fon9_API char* ToStrRev(char* pout, DayTimeSec value, FmtDef fmt);
fon9_API char* ToStrRev(char* pout, DayTimeSec value);

} // namespaces
#endif//__fon9_TimeInterval_hpp__
