// \file fon9/LogFile.cpp
// \author fonwinz@gmail.com
#include "fon9/LogFile.hpp"
#include "fon9/Log.hpp"
#include "fon9/CountDownLatch.hpp"

namespace fon9 {

LogFileAppender::~LogFileAppender() {
   this->DisposeAsync();
}
void LogFileAppender::DisposeAsync() {
   base::DisposeAsync();
   this->Timer_.StopAndWait();
}
void LogFileAppender::MakeCallForWork(WorkContentLocker& lk) {
   // Queue 多少資料時才需要 base::MakeCallForWork()?
   // - 先了解 base::MakeCallForWork() 的動作:
   //   - base::MakeCallForWork() => 如果現在為 Sleeping, 則設定 Ringing 成功 => MakeCallNow()
   //   - MakeCallNow() => {通知作業} = [建立 TakeCall() 的回呼物件, 交給 ThreadPool 處理]
   //   - {通知作業} 是很花時間的 => 盡量不呼叫 MakeCallNow(); 可大幅降低 Append() 的延遲。
   // - 少量資料時, 不要觸發 {通知作業}, 使用 Timer 觸發 MakeCallNow(); 定時將 Queue 寫入檔案。
   // - 瞬間大量資料時, 超過閥值(FlushNodeCount_), 則呼叫 base::MakeCallForWork() => 由他來考慮是否要呼叫 MakeCallNow()。
   //   - 如果現在不是 Sleeping 則不會呼叫 MakeCallNow() => Queue 處理完畢前不會再建立 TakeCall() 的回呼物件。
   //   - 所以瞬間大量資料時, 也只會觸發一次 {通知作業}
   // - 在一個 [冷系統(資料量不大)], 有時一次 {通知作業} 可能會有 [十幾 ms] 的延遲!!
   // - 所以取值原則是: 讓冷系統不要觸發 {通知作業}，但讓資料累積快速的熱系統，不要佔用太多記憶體。
   const size_t FlushNodeCount_ = 20 * 1000; // 如果1個 node 256B, 則會在大約 5M 的時候觸發 {通知作業}
   if ((FlushNodeCount_ > 0 && lk->GetQueuingNodeCount() > FlushNodeCount_) || this->IsHighWaterLevel(lk))
      base::MakeCallForWork(lk);
}
void LogFileAppender::EmitOnTimer(TimerEntry* timer, TimeStamp /*now*/) {
   LogFileAppender& rthis = ContainerOf(*static_cast<Timer*>(timer), &LogFileAppender::Timer_);
   bool isTimerRequired = true;
   rthis.Worker_.GetWorkContent([&rthis, &isTimerRequired](WorkContentLocker& lk) {
      if (lk->GetQueuingNodeCount() > 0)
         isTimerRequired = rthis.MakeCallNow(lk);
   });
   if (isTimerRequired)
      rthis.StartFlushTimer();
}

//--------------------------------------------------------------------------//

// 若在 LogFile 啟動期間會呼叫 fon9_LOG (例: DefaultTimerThread, DefaultThreadPool) 則:
// 在 LogFileImpl::Init() 會設定 gWaitLogSystemReady = &WaitLogSystemReady;
// 在呼叫 fon9_LOG 之前, 透過 if(gWaitLogSystemReady) (*gWaitLogSystemReady)(); 來等候.
static CountDownLatch LogSystemReadyLatch_{1};
static void WaitLogSystemReady() {
   LogSystemReadyLatch_.Wait();
}

class LogFileImpl : public LogFileAppender {
   fon9_NON_COPY_NON_MOVE(LogFileImpl);
   using base = LogFileAppender;

   std::string OrigStartInfo_;

   LogFileImpl(FileRotate& frConfig) {
      LogFileImpl::gLogFile = this;
      frConfig.CheckTime(UtcNow());
      SetLogWriter(&LogFileImpl::LogWriteToFile, frConfig.GetFileNameMaker().GetTimeChecker().GetTimeZoneOffset());
   }

   static void LogWriteToFile(const LogArgs& logArgs, BufferList&& buf) {
      LogFileImpl::gLogFile->CheckRotateTime(logArgs.Time_);
      LogFileImpl::gLogFile->Append(std::move(buf));
   }

   static void AddLogInfo(File& fd, RevBufferList& rbuf, TimeStamp now, char chHeadNL) {
      AddLogHeader(rbuf, now, LogLevel::Important);
      if (chHeadNL)
         RevPutChar(rbuf, chHeadNL);
      DcQueueList dcQueue{rbuf.MoveOut()};
      fd.Append(dcQueue);
   }
   virtual File* OnFileRotate(File& fd, Result openResult) override {
      bool  isFirstStart = this->OrigStartInfo_.empty();
      if (!fd.IsOpened()) {
         if (isFirstStart)
            UnsetLogWriter(&LogFileImpl::LogWriteToFile);
         fon9_LOG_ERROR("LogFile|open=", fd.GetOpenName(), "|mode=", FileModeToStr(fd.GetOpenMode()), "|err=", openResult.GetError());
         return nullptr;
      }

      TimeZoneOffset tzadj = this->GetRotateTimeChecker().GetTimeZoneOffset();
      // 避免: InitLogWriteToFile(); => SetLogWriter(others); => InitLogWriteToFile();
      // 所以這裡開檔成功後, 在設定一次 SetLogWriter(); 讓第2次的 InitLogWriteToFile(); 能順利重設 LogWriter.
      SetLogWriter(&LogFileImpl::LogWriteToFile, tzadj);

      TimeStamp      now = UtcNow() + tzadj;
      RevBufferList  rbuf{kLogBlockNodeSize};
      if (isFirstStart) {
         RevPrint(rbuf, "|orig=", fd.GetOpenName(), "|time", this->GetRotateTimeChecker().GetTimeZoneOffset(), "=", now, '\n');
         this->OrigStartInfo_ = BufferTo<std::string>(rbuf.MoveOut());
      }
      File* curfd = base::OnFileRotate(fd, openResult);
      if (curfd) {
         if (fd.IsOpened()) { // fd = 舊的 log file.
            RevPrint(rbuf, "LogFile.OnFileRotate|next=", curfd->GetOpenName(),
                     "|curr=", fd.GetOpenName(), this->OrigStartInfo_);
            this->AddLogInfo(fd, rbuf, now, '\0');
         }
         if (isFirstStart)
            RevPrint(rbuf, "LogFile.OnStart", this->OrigStartInfo_);
         else
            RevPrint(rbuf, "LogFile.OnFileRotate|prev=", fd.GetOpenName(),
                     "|curr=", curfd->GetOpenName(), this->OrigStartInfo_);
         auto fsz = curfd->GetFileSize();
         char chHeadNL = (fsz.HasResult() && fsz.GetResult() != 0) ? '\n' : '\0';
         this->AddLogInfo(*curfd, rbuf, now, chHeadNL);
      }
      return curfd;
   }

public:
   static LogFileImpl* gLogFile;
   ~LogFileImpl() {
      UnsetLogWriter(&LogFileImpl::LogWriteToFile);
      this->DisposeAsync();
      if (!this->OrigStartInfo_.empty()) {
         RevBufferList  rbuf{kLogBlockNodeSize};
         RevPrint(rbuf, "LogFile.OnDestroy", this->OrigStartInfo_);
         AddLogHeader(rbuf, UtcNow() + this->GetRotateTimeChecker().GetTimeZoneOffset(), LogLevel::Important);
         this->Append(rbuf.MoveOut());
      }
      this->Worker_.TakeCall();
      gLogFile = nullptr;
   }
   static File::Result Init(FileRotateSP frConfig, size_t highWaterLevelNodeCount) {
      if (LogFileImpl::gLogFile)
         frConfig->CheckTime(UtcNow());
      gWaitLogSystemReady = &WaitLogSystemReady;
      static intrusive_ptr<LogFileImpl> LogFile_{new LogFileImpl{*frConfig}};
      auto resfut = gLogFile->Open(std::move(frConfig), FileMode::CreatePath);
      LogSystemReadyLatch_.CountDown();
      gLogFile->SetHighWaterLevelNodeCount(highWaterLevelNodeCount);
      gLogFile->Worker_.TakeCall();//強制處理開檔要求.
      return resfut.get();
   }
};
LogFileImpl* LogFileImpl::gLogFile;

//--------------------------------------------------------------------------//

fon9_API File::Result InitLogWriteToFile(std::string fmtFileName,
                                         FileRotate::TimeScale tmScale,
                                         File::SizeType maxFileSize,
                                         size_t highWaterLevelNodeCount) {
   return LogFileImpl::Init(FileRotateSP{new FileRotate(std::move(fmtFileName), tmScale, maxFileSize)}, highWaterLevelNodeCount);
}

fon9_API bool WaitLogFileFlushed() {
   if (LogFileImpl::gLogFile)
      return LogFileImpl::gLogFile->WaitFlushed();
   return false;
}

} // namespace
