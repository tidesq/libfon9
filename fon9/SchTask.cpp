/// \file fon9/SchTask.cpp
/// \author fonwinz@gmail.com
#include "fon9/SchTask.hpp"
#include "fon9/buffer/RevBufferList.hpp"
#include "fon9/RevPrint.hpp"

namespace fon9 {

static constexpr DayTimeSec   kNoEndTime{25 * 60 * 60};
static constexpr char         kSplitField = '|';
static constexpr char         kSplitTagValue = '=';

//--------------------------------------------------------------------------//

void SchConfig::SetAlwaysInSch() {
   this->Weekdays_.set();
   this->TimeZoneName_.assign("L");
   this->TimeZoneAdj_ = GetLocalTimeZoneOffset();
   this->StartTime_.Seconds_ = 0;
   this->EndTime_ = kNoEndTime;
}

void SchConfig::Parse(StrView cfgstr) {
   this->SetAlwaysInSch();
   StrView tag, value;
   while (!cfgstr.empty()) {
      StrFetchTagValue(cfgstr, tag, value, kSplitField, kSplitTagValue);
      if (tag == "Weekdays") {
         this->Weekdays_.reset();
         for (char ch : value) {
            size_t idx = static_cast<size_t>(ch - '0');
            if (idx < kWeekdayCount)
               this->Weekdays_.set(idx);
         }
      }
      else if (tag == "Start") {
         this->StartTime_ = StrTo(value, DayTimeSec{});
      }
      else if (tag == "End") {
         this->EndTime_ = StrTo(value, kNoEndTime);
      }
      else if (tag == "TZ") {
         this->TimeZoneName_.assign(value);
         this->TimeZoneAdj_ = StrTo(value, TimeZoneOffset{});
      }
   }
}

fon9_API void RevPrint(RevBuffer& rbuf, const SchConfig& schcfg) {
   RevPrint(rbuf, kSplitField, "TZ", kSplitTagValue, schcfg.TimeZoneName_);
   if (schcfg.EndTime_ < kNoEndTime)
      RevPrint(rbuf, kSplitField, "End", kSplitTagValue, schcfg.EndTime_);
   RevPrint(rbuf, kSplitField, "Start", kSplitTagValue, schcfg.StartTime_);
   for (size_t L = kWeekdayCount; L > 0; ) {
      if (schcfg.Weekdays_.test(--L))
         RevPutChar(rbuf, static_cast<char>(L + '0'));
   }
   RevPrint(rbuf, "Weekdays", kSplitTagValue);
}

//--------------------------------------------------------------------------//

SchConfig::CheckResult SchConfig::Check(TimeStamp now) const {
   now += this->TimeZoneAdj_;
   CheckResult       res;
   const struct tm&  tm = GetDateTimeInfo(now);
   DayTimeSec        dnow{static_cast<uint32_t>((tm.tm_hour * 60 + tm.tm_min) * 60 + tm.tm_sec)};
   DayTimeSec        next = kNoEndTime;
   size_t            wday = static_cast<size_t>(tm.tm_wday);
   if (this->StartTime_ < this->EndTime_) { // 開始-結束: 在同一天.
      res.SchSt_ = (this->Weekdays_.test(wday) && (this->StartTime_ <= dnow && dnow <= this->EndTime_))
                  ? SchSt::In : SchSt::Out;
      if (res.SchSt_ == SchSt::In) // 排程時間內, 下次計時器=結束時間.
         next.Seconds_ = this->EndTime_.Seconds_ + 1;
      else if (this->Weekdays_.any()) { // 排程時間外, 今日尚未啟動: 下次計時器=(今日or次日)的開始時間.
         if (this->EndTime_ < dnow) // 排程時間外, 今日已結束: 下次計時器=次日開始時間.
            goto __CHECK_TOMORROW;
__CHECK_TODAY:
         while (!this->Weekdays_.test(wday)) {
__CHECK_TOMORROW:
            now += TimeInterval_Day(1);
            wday = (wday + 1) % 7;
         }
         next = this->StartTime_;
      }
   } else { // 結束日 = 隔日.
      if (this->StartTime_ <= dnow) { // 起始時間已到, 尚未到達隔日.
         res.SchSt_ = this->Weekdays_.test(wday) ? SchSt::In : SchSt::Out;
         if (res.SchSt_ == SchSt::In) {
            now += TimeInterval_Day(1);
            next.Seconds_ = this->EndTime_.Seconds_ + 1;
         } else if (this->Weekdays_.any())
            goto __CHECK_TOMORROW;
      }
      else { // 起始時間已到, 且已到達隔日.
         res.SchSt_ = (dnow <= this->EndTime_) ? SchSt::In : SchSt::Out;
         if (res.SchSt_ == SchSt::In) { // 尚未超過隔日結束時間.
            res.SchSt_ = this->Weekdays_.test(wday ? (wday - 1) : 6) // 看看昨日是否需要啟動.
                         ? SchSt::In : SchSt::Out;
            if (res.SchSt_ == SchSt::In)
               next.Seconds_ = this->EndTime_.Seconds_ + 1;
         }
         if (res.SchSt_ != SchSt::In && this->Weekdays_.any())
            goto __CHECK_TODAY;
      }
   }
   if (next < kNoEndTime) { // 有下次計時時間.
      res.NextCheckTime_ = TimeStampResetHHMMSS(now);
      res.NextCheckTime_ += TimeInterval_Second(next.Seconds_) - this->TimeZoneAdj_.ToTimeInterval();
   }
   return res;
}

//--------------------------------------------------------------------------//

void SchTask::Timer::EmitOnTimer(TimeStamp now) {
   SchTask&     sch = ContainerOf(*this, &SchTask::Timer_);
   Impl::Locker impl{sch.Impl_};
   bool         isCurInSch = (impl->SchState_ == SchState_InSch);
   switch (impl->SchState_) {
   case SchState_Restart:
   case SchState_InSch:
   case SchState_Stopped:
   case SchState_OutSch:    break;
   case SchState_Stopping:
   case SchState_Disposing: return;
   }
   auto res = impl->Config_.Check(now);
   if (res.NextCheckTime_.GetOrigValue() != 0)
      sch.Timer_.RunAt(res.NextCheckTime_);
   if (isCurInSch != (res.SchSt_ == SchSt::In) || impl->SchState_ == SchState_Restart) {
      impl->SchState_ = (res.SchSt_ == SchSt::In) ? SchState_InSch : SchState_OutSch;
      impl.unlock();
      sch.OnSchTask_StateChanged(res.SchSt_ == SchSt::In);
   }
}

//--------------------------------------------------------------------------//

SchTask::~SchTask() {
   this->Timer_.DisposeAndWait();
}
bool SchTask::SetSchState(SchState st) {
   Impl::Locker   impl{this->Impl_};
   if (impl->SchState_ >= SchState_Disposing)
      return false;
   impl->SchState_ = st;
   return true;
}
bool SchTask::Restart(TimeInterval ti) {
   Impl::Locker impl{this->Impl_};
   switch (impl->SchState_) {
   case SchState_Restart:
   case SchState_InSch:
   case SchState_Stopped:
   case SchState_OutSch:    break;
   case SchState_Stopping:
   case SchState_Disposing: return false;
   }
   impl->SchState_ = SchState_Restart;
   this->Timer_.RunAfter(ti);
   return true;
}
bool SchTask::Start(const StrView& cfgstr) {
   TimeStamp    now = UtcNow();
   Impl::Locker impl{this->Impl_};
   bool         isCurInSch = (impl->SchState_ == SchState_InSch);
   impl->Config_.Parse(cfgstr);
   switch (impl->SchState_) {
   case SchState_InSch:
   case SchState_Stopped:
   case SchState_OutSch:    break;
   case SchState_Stopping:
   case SchState_Disposing: return false;
   case SchState_Restart:   return true;
   }
   auto  res = impl->Config_.Check(now);
   if (isCurInSch != (res.SchSt_ == SchSt::In))
      res.NextCheckTime_ = now + TimeInterval_Second(1);
   else if (res.NextCheckTime_.GetOrigValue() == 0) {//sch狀態正確 && 沒有結束時間: 不用啟動計時器.
      this->Timer_.StopNoWait();
      return true;
   }
   this->Timer_.RunAt(res.NextCheckTime_);
   return true;
}
void SchTask::StopAndWait() {
   if (!this->SetSchState(SchState_Stopping))
      return;
   this->Timer_.StopAndWait();
   this->SetSchState(SchState_Stopped);
}
void SchTask::DisposeAndWait() {
   this->SetSchState(SchState_Disposing);
   this->Timer_.DisposeAndWait();
}

} // namespaces
