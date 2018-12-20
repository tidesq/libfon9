/// \file fon9/LogFile.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_LogFile_hpp__
#define __fon9_LogFile_hpp__
#include "fon9/FileAppender.hpp"
#include "fon9/Timer.hpp"

namespace fon9 {

/// \ingroup Misc
/// \param fmtFileName 格式 "{0}{1}", 參數0 = 時間(TimeStamp), 參數1 = 序號(正整數)。
///   - 若 fmtFileName 時間格式有設定 TimeZone(e.g. "{0:+8}"), 則記錄訊息的時間也會跟著調整。
/// \param tmScale 檔案時間切換刻度
/// \param maxFileSize >0 時才有效
///   - 每次寫完 AppendBuffer 時檢查一次檔案大小, 超過此值則更換檔案, 檔名格式必須有{1}序號參數。
///   - 實際檔案大小可能會超過: 最後換檔前那次 AppendBuffer 的資料量。
/// \param  highWaterLevelNodeCount > 0: 當尚未寫入的資料量超過 highWaterLevelNodeCount:
///   - fon9_LOG_() 會等候資料消化後才會返回。
///   - TODO: 可選擇: 拋棄之後的log訊息.
/// \return File::Open() 的結果.
fon9_API File::Result InitLogWriteToFile(std::string fmtFileName,
                                         FileRotate::TimeScale tmScale,
                                         File::SizeType maxFileSize,
                                         size_t highWaterLevelNodeCount);

fon9_API bool WaitLogFileFlushed();

/// \ingroup Misc
/// - 寫檔時機:
///   - 每隔 n 秒: 透過 SetFlushInterval() 設定, 預設為 1 秒.
///   - 資料節點數量 > m 個.
class fon9_API LogFileAppender : public AsyncFileAppender {
   fon9_NON_COPY_NON_MOVE(LogFileAppender);
   using base = AsyncFileAppender;

   static void EmitOnTimer(TimerEntry* timer, TimeStamp now);
   using Timer = DataMemberEmitOnTimer<&LogFileAppender::EmitOnTimer>;
   Timer          Timer_{GetDefaultTimerThread()};
   TimeInterval   FlushInterval_{TimeInterval_Second(1)};
   void StartFlushTimer() {
      this->Timer_.RunAfter(this->FlushInterval_);
   }

protected:
   LogFileAppender() {
      this->StartFlushTimer();
   }
   LogFileAppender(FileMode fm) : base{fm} {
      this->StartFlushTimer();
   }
   ~LogFileAppender();

   virtual void DisposeAsync() override;

   /// 資料節點數量 > m 個, 才會呼叫 this->MakeCallNow(lk); 否則 do nothing.
   virtual void MakeCallForWork(WorkContentLocker&&) override;

public:
   void SetFlushInterval(TimeInterval ti) {
      this->FlushInterval_ = ti;
   }

   template <class... ArgsT>
   static AsyncFileAppenderSP Make(ArgsT&&... args) {
      return AsyncFileAppenderSP{new LogFileAppender{std::forward<ArgsT>(args)...}};
   }
};

} // namespace
#endif//__fon9_LogFile_hpp__
