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
/// 字串轉成 TimeInterval, 支援的格式參考 \ref char* ToStrRev(char* pout, TimeInterval ti);
fon9_API TimeInterval StrTo(const StrView& str, TimeInterval value = TimeInterval::Null(), const char** endptr = nullptr);

} // namespaces
#endif//__fon9_TimeInterval_hpp__
