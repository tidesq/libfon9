// \file fon9/TimedFileName.cpp
// \author fonwinz@gmail.com
#include "fon9/TimedFileName.hpp"
#include "fon9/buffer/RevBufferList.hpp"

namespace fon9 {

void TimeChecker::Reset(TimeStamp tm) {
   this->UtcTime_ = tm;
   tm += this->TimeZoneOffset_;
   this->AdjustedYMDHMS_ = ToYYYYMMDDHHMMSS(tm);
   if (this->Scale_ > TimeScale::No)
      this->AdjustedYMDHMS_ /= static_cast<DateTime14T>(this->Scale_);
}

bool TimeChecker::CheckTime(TimeStamp tm) {
   if (this->Scale_ <= TimeScale::No) {
      if (this->UtcTime_ != TimeStamp{})
         return false;
      this->UtcTime_ = tm;
      return true;
   }
   TimeInterval difs = tm - this->UtcTime_;
   if (TimeInterval_Second(-60) < difs && difs <= TimeInterval{})
      // 時間小幅往前, 視為誤差, 不改變現在狀態.
      return false;
   this->UtcTime_ = tm;
   tm += this->TimeZoneOffset_;
   DateTime14T ymdhms = ToYYYYMMDDHHMMSS(tm) / static_cast<DateTime14T>(this->Scale_);
   if (this->AdjustedYMDHMS_ == ymdhms)
      return false;
   this->AdjustedYMDHMS_ = ymdhms;
   return true;
}

//--------------------------------------------------------------------------//

TimedFileName::TimedFileName(std::string fmtFileName, TimeScale tmScale)
   : FmtFileName_{std::move(fmtFileName)} {
   StrView tmfmt;
   TimeZoneOffset tzadj;
   if (this->FmtFileName_.GetArgIdFormat(0u, tmfmt))
      tzadj = FmtTS{tmfmt}.TimeZoneOffset_;
   this->TimeChecker_.Reset(tmScale, tzadj);
}
bool TimedFileName::RebuildFileName(TimeStamp tm) {
   this->SeqNo_ = 0;
   this->TimeChecker_.Reset(tm);
   return this->RebuildFileName();
}
bool TimedFileName::CheckTime(TimeStamp tm) {
   if (!this->TimeChecker_.CheckTime(tm))
      return false;
   if (!this->RebuildFileName()) { // 時間變了, 但檔名沒變: 檔名格式與 Scale_ 不搭配!
      if (this->FmtFileName_.GetMaxArgId() >= 1) // 可以依靠 SeqNo 來改變檔名.
         return this->AddSeqNo();
      return false;
   }
   if (this->SeqNo_ != 0 && this->FmtFileName_.GetMaxArgId() >= 1) {
      this->SeqNo_ = 0;
      this->RebuildFileName();
   }
   return true;
}
bool TimedFileName::RebuildFileName() {
   RevBufferList   rbuf{128};
   this->FmtFileName_(rbuf, this->TimeChecker_.GetUtcTime(), this->SeqNo_);
   std::string fnNew = BufferTo<std::string>(rbuf.MoveOut());
   if (this->FileName_ == fnNew)
      return false;
   this->FileName_ = std::move(fnNew);
   return true;
}

} // namespace
