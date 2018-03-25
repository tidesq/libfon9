/// \file fon9/TimedFileName.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_TimedFileName_hpp__
#define __fon9_TimedFileName_hpp__
#include "fon9/RevFormat.hpp"
#include "fon9/TimeStamp.hpp"

namespace fon9 {

fon9_WARN_DISABLE_PADDING;
fon9_MSC_WARN_DISABLE_NO_PUSH(4251);// dll-interface.
/// \ingroup Misc
/// 在指定時間測量單位(TimeChecker::TimeScale)之下, 檢查時間是否有變動.
class fon9_API TimeChecker {
   TimeStamp   Time_{};
   DateTime14T YMDHMS_{};
   void Clear() {
      this->Time_ = TimeStamp{};
      this->YMDHMS_ = DateTime14T{};
   }
public:
   TimeChecker() = default;
   TimeChecker(TimeChecker&& rhs) : Time_{rhs.Time_}, YMDHMS_{rhs.YMDHMS_} {
      rhs.Clear();
   }
   TimeChecker& operator=(TimeChecker&& rhs) {
      this->Time_ = rhs.Time_;
      this->YMDHMS_ = rhs.YMDHMS_;
      rhs.Clear();
      return *this;
   }
   
   TimeChecker(const TimeChecker&) = default;
   TimeChecker& operator=(const TimeChecker&) = default;

   TimeStamp GetTime() const {
      return this->Time_;
   }

   /// **何時** 需要開啟新的檔案?
   enum class TimeScale : DateTime14T {
      /// 檔名切換與時間變動無關.
      /// 只有首次 CheckTime() 會傳回 true.
      No = 0,
      /// 秒數有變就切換: yyyymmddHHMMSS 有變動就換檔名.
      Second = 1,
      /// 分鐘數有變就切換: yyyymmddHHMM 有變動就換檔名.
      /// 例: 上次的檔名時間是: 20160904-123456, 現在的時間是 20160904-123500,
      /// 雖然時間只經過了4秒, 但 CheckTime() 會傳回 true.
      Minute = Second * 100,
      /// 小時數有變就切換: yyyymmddHH 有變動就換檔名.
      Hour = Minute * 100,
      /// 日期有變就切換: yyyymmdd 有變動就換檔名.
      Day = Hour * 100,
      /// 月份有變就切換: yyyymm 有變動就換檔名.
      Month = Day * 100,
      /// 年份有變就切換: yyyy 有變動就換檔名.
      Year = Month * 100,
   };

   void Reset(TimeScale tmScale, TimeStamp tm, TimeZoneOffset tzadj);

   /// 與上次時間差距在 n(-60秒..0秒) 秒內, 視為同一時間.
   /// 避免: 從不同地方收集的時間誤差, 例:
   /// - 情況一:
   ///    - Thr1:取得時間tm1
   ///    - Thr2:取得時間tm2, 立即(也許0.01秒)呼叫 CheckTime(tm2) => 使用 tm2 產生檔名.
   ///    - Thr1:在tm1之後忙了一會兒(也許2秒)之後 CheckTime(tm1)
   ///    - 此時的檔名已變成 tm2 產生的, 不應再切回 tm1 的檔名.
   /// - 情況二:
   ///    - 系統異常, 時間被調到 1 個月之後.
   ///    - 發現異常後, 調回正常時間
   ///    - 因此不能只用時間先後來判斷是否切換檔名.
   ///
   /// \retval true  時間有變動.
   /// \retval false 時間沒變動.
   bool CheckTime(TimeScale tmScale, TimeStamp tm, TimeZoneOffset tzadj);
};

/// \ingroup Misc
/// 根據時間變動, 判斷是否需要建立新的檔名.
class fon9_API TimedFileName {
   fon9_NON_COPY_NON_MOVE(TimedFileName);
public:
   using TimeScale = TimeChecker::TimeScale;

   /// fmt = "{0}{1}", 參數0=時間(TimeStamp), 參數1=序號(正整數).
   TimedFileName(std::string fmt, TimeScale tmScale);
   TimedFileName(TimedFileName&& rhs) {
      *this = std::move(rhs);
   }
   TimedFileName& operator=(TimedFileName&& rhs) {
      this->FnFormat_ = std::move(rhs.FnFormat_);
      this->Scale_ = rhs.Scale_;
      this->TimeZoneOffset_ = rhs.TimeZoneOffset_;
      this->TimeChecker_ = std::move(rhs.TimeChecker_);
      this->SeqNo_ = rhs.SeqNo_;
      this->FileName_ = std::move(rhs.FileName_);
      rhs.SeqNo_ = 0;
      return *this;
   }

   TimeScale GetTimeScale() const {
      return this->Scale_;
   }
   TimeZoneOffset GetTimeZoneOffset() const {
      return this->TimeZoneOffset_;
   }
   TimeStamp GetTime() const {
      return this->TimeChecker_.GetTime();
   }

   /// 檢查是否需要重建檔名, 若有需要則返回 true, 並在返回前重建檔名.
   /// - 如果 Scale 與 fmt 沒有完全搭配時, 則自動增加序號, 例:
   ///   - TimeScale::Minute, fmt="./log/{0:f-H}.{1:04}.log" (時間格式=YYYYMMDD-HH (排除MMSS), 序號使用"04"格式)
   ///   - 當分鐘數改變時會用增加序號的方式建立新檔名.
   ///     - 例: tm=20160904-101234, FileName="./log/20160904-10.0000.log"
   ///     - 例: tm=20160904-101300, FileName="./log/20160904-10.0001.log"
   /// \code
   ///   if (tfn.CheckTime(tm)) {
   ///      if (fd.GetOpenName() != tfn.GetFileName()) {
   ///         File nfd(tfn.GetFileName()...);
   ///         ...
   ///         fd = std::move(nfd);
   ///      }
   ///   }
   /// \endcode
   ///
   /// \retval true  時間有變動, 此時序號會重新設為0, 並重建新檔名.
   /// \retval false 時間沒變動, 不須重新建立檔名.
   bool CheckTime(TimeStamp tm);

   /// 在 CheckTime(tm) 之後, 若時間有變動, 則取得使用 tm 建立的新檔名.
   const std::string& GetFileName() const {
      return this->FileName_;
   }

   /// 增加檔名序號.
   /// - 在某些情況下(例:檔案大小超過設定值), 應該要開啟新檔, 但時間沒有變動. 此時就可以使用序號功能.
   /// - 要使用此功能, 檔名格式必須提供參數{1}.
   /// - 此序號在 CheckTime() 傳回 true 時會歸0.
   /// \retval true  檔名有變動.
   /// \retval false 檔名沒變動: 檔名格式沒有提供參數{1}
   bool AddSeqNo() {
      ++this->SeqNo_;
      return RebuildFileName();
   }

   /// 強制使用指定時間重建檔名.
   /// \retval true 檔名有異動.
   bool RebuildFileName(TimeStamp tm);

private:
   bool RebuildFileName();
   FmtPre         FnFormat_;
   TimeScale      Scale_;
   TimeZoneOffset TimeZoneOffset_;
   TimeChecker    TimeChecker_;
   size_t         SeqNo_{};
   std::string    FileName_;
};
fon9_WARN_POP;

} // namespace
#endif//__fon9_TimedFileName_hpp__
