// \file fon9/TimedFileName.cpp
// \author fonwinz@gmail.com
#include "fon9/TimedFileName.hpp"
#include "fon9/buffer/RevBufferList.hpp"

namespace fon9 {

void TimeChecker::Reset(TimeScale tmScale, TimeStamp tm, TimeZoneOffset tzadj) {
   this->Time_ = tm;
   tm += tzadj;
   this->YMDHMS_ = ToYYYYMMDDHHMMSS(tm);
   if (tmScale > TimeScale::No)
      this->YMDHMS_ /= static_cast<DateTime14T>(tmScale);
}

bool TimeChecker::CheckTime(TimeScale tmScale, TimeStamp tm, TimeZoneOffset tzadj) {
   if (tmScale <= TimeScale::No) {
      if (this->Time_ != TimeStamp{})
         return false;
      this->Time_ = tm;
      return true;
   }
   TimeInterval difs = tm - this->Time_;
   if (TimeInterval_Second(-60) < difs && difs <= TimeInterval{})
      // 時間小幅往前, 視為誤差, 不改變現在狀態.
      return false;
   this->Time_ = tm;
   tm += tzadj;
   DateTime14T ymdhms = ToYYYYMMDDHHMMSS(tm) / static_cast<DateTime14T>(tmScale);
   if (this->YMDHMS_ == ymdhms)
      return false;
   this->YMDHMS_ = ymdhms;
   return true;
}

//--------------------------------------------------------------------------//

TimedFileName::TimedFileName(std::string fmt, TimeScale tscale)
   : FnFormat_{std::move(fmt)}
   , Scale_{tscale} {
   StrView tmfmt;
   if (this->FnFormat_.GetArgIdFormat(0u, tmfmt))
      this->TimeZoneOffset_ = FmtTS{tmfmt}.TimeZoneOffset_;
}
bool TimedFileName::RebuildFileName(TimeStamp tm) {
   this->SeqNo_ = 0;
   this->TimeChecker_.Reset(this->Scale_, tm, this->TimeZoneOffset_);
   return this->RebuildFileName();
}
bool TimedFileName::CheckTime(TimeStamp tm) {
   if (!this->TimeChecker_.CheckTime(this->Scale_, tm, this->TimeZoneOffset_))
      return false;
   if (!this->RebuildFileName()) { // 時間變了, 但檔名沒變: 檔名格式與 Scale_ 不搭配!
      if (this->FnFormat_.GetMaxArgId() >= 1) // 可以依靠 SeqNo 來改變檔名.
         return this->AddSeqNo();
      return false;
   }
   if (this->SeqNo_ != 0 && this->FnFormat_.GetMaxArgId() >= 1) {
      this->SeqNo_ = 0;
      this->RebuildFileName();
   }
   return true;
}
bool TimedFileName::RebuildFileName() {
   RevBufferList   rbuf{128};
   this->FnFormat_(rbuf, this->TimeChecker_.GetTime(), this->SeqNo_);
   std::string fnNew = BufferTo<std::string>(rbuf.MoveOut());
   if (this->FileName_ == fnNew)
      return false;
   this->FileName_ = std::move(fnNew);
   return true;
}

} // namespace
