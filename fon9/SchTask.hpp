/// \file fon9/SchTask.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_SchTask_hpp__
#define __fon9_SchTask_hpp__
#include "fon9/Timer.hpp"
#include "fon9/buffer/RevBuffer.hpp"
#include <bitset>

namespace fon9 {

fon9_MSC_WARN_DISABLE(4820);
enum class SchSt {
   Out = 0,
   In = 1,
   Unknown = -1,
};

/// \ingroup Thrs
/// 排程設定.
struct fon9_API SchConfig {
   /// 星期的設定, 以起始時間(StartTime_)的星期幾為準.
   std::bitset<kWeekdayCount>  Weekdays_;
   /// 排程設定(時間、星期)的 TimeZone 調整值.
   TimeZoneOffset TimeZoneAdj_{};
   CharVector     TimeZoneName_;
   /// in sch:
   /// - StartTime_ < EndTime_:  結束日=開始日:   (StartTime_ <= now && now <= EndTime_)
   /// - StartTime_ >= EndTime_: 結束日=開始日+1: (StartTime_ <= now || now <= EndTime_)
   DayTimeSec  StartTime_{};
   DayTimeSec  EndTime_{};

   bool operator==(const SchConfig& rhs) const {
      return this->Weekdays_ == rhs.Weekdays_
         && this->TimeZoneAdj_ == rhs.TimeZoneAdj_
         && this->TimeZoneName_ == rhs.TimeZoneName_
         && this->StartTime_ == rhs.StartTime_
         && this->EndTime_ == rhs.EndTime_;
   }
   bool operator!=(const SchConfig& rhs) const {
      return !(*this == rhs);
   }

   struct CheckResult {
      TimeStamp NextCheckTime_;
      SchSt     SchSt_;
   };

   SchConfig() {
      this->SetAlwaysInSch();
   }
   SchConfig(const StrView& cfgstr) {
      this->Parse(cfgstr);
   }

   /// 檢查 now 是否在排程時間內 & 計算下次檢查時間.
   CheckResult Check(TimeStamp now) const;

   /// 設定一直都在時間內 & 沒有結束時間.
   /// - "Weekdays=0123456|Start=00:00:00|End=|TZ=本機localtime offset(GetLocalTimeZoneOffset())"
   void SetAlwaysInSch();

   /// 先設定成預設值(永遠在時間內), 然後解析 cfgstr.
   void Parse(StrView cfgstr);
};

fon9_API void RevPrint(RevBuffer& rbuf, const SchConfig& schcfg);

/// \ingroup Thrs
/// 排程項目.
class fon9_API SchTask {
   fon9_NON_COPY_NON_MOVE(SchTask);
   struct Timer : public DataMemberTimer {
      fon9_NON_COPY_NON_MOVE(Timer);
      Timer() = default;
      virtual void EmitOnTimer(TimeStamp now) override;
   };
   Timer Timer_;
   enum SchState {
      SchState_Restart,
      SchState_Stopped,
      SchState_Stopping,
      SchState_InSch,
      SchState_OutSch,
      SchState_Disposing,
   };
   struct ImplBase {
      SchConfig   Config_;
      SchState    SchState_{SchState_Stopped};
   };
   using Impl = MustLock<ImplBase>;
   Impl  Impl_;
   bool SetSchState(SchState st);
   /// 當 [in sch => out sch] or [out sch => in sch] 時觸發事件.
   /// 在 Timer thread 觸發事件, 觸發時沒有鎖住任何東西.
   virtual void OnSchTask_StateChanged(bool isInSch) = 0;
public:
   SchTask() = default;
   virtual ~SchTask();

   /// 啟動 SchTask.
   /// 剛建立(或停止後)的 SchTask 一律在 SchOut 狀態, 所以剛建立(或停止後)的第一次觸發將會是 sch in 事件.
   /// \retval false 已呼叫過 DisposeAndWait(), 或正在 StopAndWait();
   bool Start(const StrView& cfgstr);

   /// 強迫在 ti 之後, 再次觸發 OnSchTask_StateChanged() 事件.
   /// \retval false 已呼叫過 DisposeAndWait(), 或正在 StopAndWait();
   bool Restart(TimeInterval ti);

   /// 停止排程.
   void StopAndWait();
   void DisposeAndWait();
};
fon9_MSC_WARN_POP;

} // namespaces
#endif//__fon9_SchTask_hpp__
