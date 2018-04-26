/// \file fon9/FileAppender.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_FileAppender_hpp__
#define __fon9_FileAppender_hpp__
#include "fon9/Appender.hpp"
#include "fon9/File.hpp"
#include "fon9/intrusive_ref_counter.hpp"
#include "fon9/TimedFileName.hpp"

fon9_BEFORE_INCLUDE_STD;
#include <future>
fon9_AFTER_INCLUDE_STD;

namespace fon9 {

fon9_WARN_DISABLE_PADDING;

class FileRotate {
   fon9_NON_COPY_NON_MOVE(FileRotate);
   // 不保證會剛好在此大小時換檔, 一般會在每次 Append 完成後檢查。
   File::SizeType MaxFileSize_;
   TimedFileName  FileNameMaker_;
public:
   using TimeScale = TimeChecker::TimeScale;
   FileRotate(std::string fmtFileName, TimeScale tmScale, File::SizeType maxFileSize = 0)
      : MaxFileSize_{maxFileSize}
      , FileNameMaker_(std::move(fmtFileName), tmScale) {
   }
   const TimedFileName& GetFileNameMaker() const {
      return this->FileNameMaker_;
   }
   bool IsCheckFileSizeRequired() const {
      return this->MaxFileSize_ > 0;
   }
   const std::string* CheckFileSize(File::SizeType currFileSize) {
      if (this->MaxFileSize_ > 0 && currFileSize >= this->MaxFileSize_) {
         if (this->FileNameMaker_.AddSeqNo())
            return &this->FileNameMaker_.GetFileName();
      }
      return nullptr;
   }
   const std::string* CheckTime(TimeStamp tm) {
      if (this->FileNameMaker_.CheckTime(tm))
         return &this->FileNameMaker_.GetFileName();
      return nullptr;
   }
};
using FileRotateSP = std::unique_ptr<FileRotate>;

/// \ingroup Misc
/// - 當收到 Append() 要求時, 立即寫檔.
/// - 如果同時有多個 thread 呼叫 Append(); 則可能會集中在第一個呼叫 Append() 的 thread 寫檔.
class fon9_API FileAppender : public Appender {
   fon9_NON_COPY_NON_MOVE(FileAppender);
   FileRotateSP   FileRotate_;
   TimeChecker    RotateTimeChecker_;
   File           File_;

   struct NodeReopen;
   struct NodeCheckRotateTime;
   File::Result CheckRotateReopen(std::string newfn, FileMode fmode);
protected:
   /// fd 不一定開檔成功, 若失敗, 則可從 openResult 取得原因.
   /// 預設: 不論 fd 是否有開檔成功, 一律 std::swap(this->File_, fd); 並返回 &this->File_;
   virtual File* OnFileRotate(File& fd, File::Result openResult);
   /// 預設為同步模式: 立即呼叫 TakeCall(); 並返回 true.
   virtual bool MakeCallNow(WorkContentLocker& lk) override;
   virtual void ConsumeAppendBuffer(DcQueueList& buffer) override;

   const TimeChecker& GetRotateTimeChecker() const {
      return this->RotateTimeChecker_;
   }

public:
   using Result = File::Result;
   using SizeType = File::SizeType;
   using PosType = File::PosType;

   FileAppender() = default;
   FileAppender(FileMode fm) : File_{fm | FileMode::Append} {
   }

   /// 解構時, 如果 Worker_ 的狀態不是 WorkerState::Disposed 則:
   /// 寫入剩餘的資料, 避免資料遺失.
   ~FileAppender();

   using OpenResult = std::future<Result>;
   OpenResult Open(std::string fname, FileMode fmode);
   OpenResult Open(FileRotateSP&& frConfig, FileMode fmode);
   void CheckRotateTime(TimeStamp tm);

   void Close() {
      this->WaitFlushed();
      return this->File_.Close();
   }
   Result GetFileSize() {
      this->WaitFlushed();
      return this->File_.GetFileSize();
   }
   Result Read(PosType offset, void* buf, SizeType count) {
      this->WaitFlushed();
      return this->File_.Read(offset, buf, count);
   }
};

/// \ingroup Misc
/// - 當收到 Append() 要求時, 丟到 DefaultThreadPool 寫檔.
class fon9_API AsyncFileAppender : public intrusive_ref_counter<AsyncFileAppender>, public FileAppender {
   fon9_NON_COPY_NON_MOVE(AsyncFileAppender);
   using base = FileAppender;

   size_t   HighWaterLevelNodeCount_{0};

protected:
   using base::base;
   AsyncFileAppender() = default;

   /// 停止 Async: 把狀態設為 WorkerState::Disposing;
   virtual void DisposeAsync();

   /// \retval true  則把需求丟到 DefaultThreadPool 去處理.
   /// \retval false 現在狀態無法進行非同步要求(下班了? 正在結構?).
   virtual bool MakeCallNow(WorkContentLocker& lk) override;
   /// - 如果 IsHighWaterLevel() => WaitConsumed() 等候水位降低.
   /// - 否則透過底層處理: 若現在狀態 == WorkerState::Sleeping, 則呼叫 MakeCallNow();
   virtual void MakeCallForWork(WorkContentLocker& lk) override;

   bool IsHighWaterLevel(WorkContentLocker& lk) {
      return (this->HighWaterLevelNodeCount_ > 0 && lk->GetTotalNodeCount() >= this->HighWaterLevelNodeCount_);
   }

public:
   ~AsyncFileAppender();

   /// 當剩餘節點數量超過 HighWaterLevelNodeCount 則進行管制:
   /// - 等候消化到高水位管制解除?
   /// - 拋棄新加入的資料?
   void SetHighWaterLevelNodeCount(size_t highWaterLevelNodeCount) {
      this->HighWaterLevelNodeCount_ = highWaterLevelNodeCount;
   }

   using AsyncFileAppenderSP = intrusive_ptr<AsyncFileAppender>;

   template <class... ArgsT>
   static AsyncFileAppenderSP Make(ArgsT&&... args) {
      return AsyncFileAppenderSP{new AsyncFileAppender{std::forward<ArgsT>(args)...}};
   }
};
using AsyncFileAppenderSP = AsyncFileAppender::AsyncFileAppenderSP;

fon9_WARN_POP;
} // namespaces
#endif//__fon9_FileAppender_hpp__
