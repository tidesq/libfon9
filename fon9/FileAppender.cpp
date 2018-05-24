/// \file fon9/FileAppender.cpp
/// \author fonwinz@gmail.com
#include "fon9/FileAppender.hpp"
#include "fon9/DefaultThreadPool.hpp"

namespace fon9 {

fon9_WARN_DISABLE_PADDING;
struct FileAppender::NodeReopen : public BufferNodeVirtual {
   fon9_NON_COPY_NON_MOVE(NodeReopen);
   using base = BufferNodeVirtual;
   friend class BufferNode;// for BufferNode::Alloc();
   using base::base;
   FileAppender*  FileAppender_;
   FileMode       FileMode_;
   std::string    FileName_;
   FileRotateSP   FileRotate_;
   std::promise<File::Result> OpenResult_;
protected:
   NodeReopen(BufferNodeSize blockSize, FileAppender* owner, std::string&& fname, FileMode fmode)
      : base(blockSize, StyleFlag{})
      , FileAppender_(owner)
      , FileMode_{fmode}
      , FileName_{std::move(fname)} {
   }
   NodeReopen(BufferNodeSize blockSize, FileAppender* owner, FileRotateSP&& frotate, FileMode fmode)
      : base(blockSize, StyleFlag{})
      , FileAppender_(owner)
      , FileMode_{fmode}
      , FileRotate_{std::move(frotate)} {
   }
   virtual void OnBufferConsumed() override {
      this->ReopenOwner();
   }
   virtual void OnBufferConsumedErr(const ErrC&) override {
      this->ReopenOwner();
   }
   void ReopenOwner() {
      if (this->FileRotate_) {
         this->FileName_ = this->FileRotate_->GetFileNameMaker().GetFileName();
         this->FileAppender_->RotateTimeChecker_ = this->FileRotate_->GetFileNameMaker().GetTimeChecker();
         this->FileAppender_->FileRotate_ = std::move(this->FileRotate_);
      }
      File::Result res = this->FileAppender_->CheckRotateReopen(this->FileName_, this->FileMode_ | FileMode::Append);
      this->OpenResult_.set_value(res);
   }
public:
   template <class T>
   static OpenResult AddNode(FileAppender& owner, T&& fname, FileMode fmode) {
      NodeReopen* node = base::Alloc<NodeReopen>(0, &owner, std::move(fname), fmode);
      OpenResult  res = node->OpenResult_.get_future();
      owner.Append(node);
      return res;
   }
};

//--------------------------------------------------------------------------//

struct FileAppender::NodeCheckRotateTime : public BufferNodeVirtual {
   fon9_NON_COPY_NON_MOVE(NodeCheckRotateTime);
   using base = BufferNodeVirtual;
   friend class BufferNode;// for BufferNode::Alloc();
   using base::base;
   FileAppender*  FileAppender_;
   TimeStamp      Time_;
protected:
   NodeCheckRotateTime(BufferNodeSize blockSize, FileAppender* owner, TimeStamp tm)
      : base(blockSize, StyleFlag{})
      , FileAppender_(owner)
      , Time_{tm} {
   }
   virtual void OnBufferConsumed() override {
      this->CheckRotateTime();
   }
   virtual void OnBufferConsumedErr(const ErrC&) override {
      this->CheckRotateTime();
   }
   void CheckRotateTime() {
      if (this->FileAppender_->FileRotate_)
         if (const std::string* newfn = this->FileAppender_->FileRotate_->CheckTime(this->Time_))
            this->FileAppender_->CheckRotateReopen(*newfn, this->FileAppender_->File_.GetOpenMode());
   }
public:
   static void AddNode(FileAppender& owner, TimeStamp tm) {
      owner.Append(base::Alloc<NodeCheckRotateTime>(0, &owner, tm));
   }
};
fon9_WARN_POP;

//--------------------------------------------------------------------------//

FileAppender::~FileAppender() {
   this->Worker_.TakeCall();
}
FileAppender::OpenResult FileAppender::Open(std::string fname, FileMode fmode) {
   return NodeReopen::AddNode(*this, std::move(fname), fmode);
}
FileAppender::OpenResult FileAppender::Open(FileRotateSP&& frConfig, FileMode fmode) {
   return NodeReopen::AddNode(*this, std::move(frConfig), fmode);
}
void FileAppender::CheckRotateTime(TimeStamp tm) {
   // 在 thread 環境下 this->RotateTimeChecker_ 不安全, 但不影響換檔的正確性:
   // 因為這裡僅是檢查 [是否可能需要] 換檔, 實際換檔操作是在 NodeCheckRotateTime.
   // this->RotateTimeChecker_ 內容即使因 thread 而混亂, 只是會瞬間建立多個 NodeCheckRotateTime.
   // 在後續的 this->RotateTimeChecker_.CheckTime() 之後, this->RotateTimeChecker_ 最終總會正確.
   if (this->RotateTimeChecker_.CheckTime(tm))
      NodeCheckRotateTime::AddNode(*this, tm);
}

bool FileAppender::MakeCallNow(WorkContentLocker& lk) {
   this->Worker_.TakeCallLocked(lk);
   return true;
}
void FileAppender::ConsumeAppendBuffer(DcQueueList& buffer) {
   this->File_.Append(buffer);
   if (this->FileRotate_ && this->FileRotate_->IsCheckFileSizeRequired()) {
      if (Result fsz = this->File_.GetFileSize())
         if (const std::string* newfn = FileRotate_->CheckFileSize(fsz.GetResult()))
            this->CheckRotateReopen(*newfn, this->File_.GetOpenMode());
   }
}
File::Result FileAppender::CheckRotateReopen(std::string newfn, FileMode fmode) {
   if (this->File_.IsOpened() && this->File_.GetOpenName() == newfn)
      return File::Result{0};
   File  newfd;
   for (;;) {
      Result openResult = newfd.Open(newfn, fmode);
      if (newfd.IsOpened() && this->FileRotate_ && this->FileRotate_->IsCheckFileSizeRequired()) {
         if (Result fsz = newfd.GetFileSize())
            if (const std::string* nextfn = this->FileRotate_->CheckFileSize(fsz.GetResult())) {
               if (newfn != *nextfn) {
                  newfn = *nextfn;
                  continue;
               }
            }
      }
      this->OnFileRotate(newfd, openResult);
      return openResult;
   }
}
File* FileAppender::OnFileRotate(File& fd, Result /*openResult*/) {
   std::swap(this->File_, fd);
   return &this->File_;
}

//--------------------------------------------------------------------------//

AsyncFileAppender::~AsyncFileAppender() {
   this->DisposeAsync();
}
void AsyncFileAppender::DisposeAsync() {
   this->Worker_.Dispose();
}
bool AsyncFileAppender::MakeCallNow(WorkContentLocker& lk) {
   if (!lk->SetToAsyncTaking())
      return true;
   lk.unlock();

   //>>>>>
   //if (this->use_count() == 0)// => 此時必定正在解構(或正要進行解構).
   //   return false;           // 為了避免在解構時, 又呼叫 MakeCallNow() 造成重複解構!
   //(*) 如果在此時最後一個 ptr 死了...
   //    最後這次的 MakeCallNow() 是哪來的呢? Timer?
   //intrusive_ptr_add_ref(this);
   //=====
   //* 由於有 AsyncTaking 旗標的保護，所以這裡就不用判斷 use_count()==0 了!
   //* 改成判斷 intrusive_ptr_add_ref() 的傳回值(加一之前的值):
   //  如果加一前 ref counter == 0, 則表示 this 已經進入死亡程序，
   //  此時不可執行 async，但保留 AsyncTaking==true: 讓後進者在 unlock() 之前就離開。
   if (intrusive_ptr_add_ref(this) == 0)
      return false;
   // => 理論上, 應該要在解構過程等候 [此次MakeCallNow()的呼叫者(例如: LogFile 裡面的 Timer_)] 結束。
   // => 這招只能用在只有一個額外 user(例如: LogFile 裡面的 Timer_) 的場合。
   //<<<<<
   AsyncFileAppenderSP pthis(this, false); // false: 不用再 add_ref().

   // 為了避免 EmplaceMessage() 失敗 or 程式結束時沒有執行加入的 task,
   // 所以在此先釋放上面的 if (intrusive_ptr_add_ref(this) == 0)...
   // 並使用 intrusive_ptr<> pthis 傳遞, 這樣才能確保當要求的 task 沒有執行時, this 仍會正常死亡!

   GetDefaultThreadPool().EmplaceMessage([pthis]() {
      pthis->Worker_.GetWorkContent([&pthis](WorkContentLocker& lk2) {
         lk2->SetAsyncTaken();
         pthis->Worker_.TakeCallLocked(lk2);
      });
   });
   return true;
}
void AsyncFileAppender::MakeCallForWork(WorkContentLocker& lk) {
   if (!this->IsHighWaterLevel(lk)) {
      base::MakeCallForWork(lk);
      return;
   }
   do {
      if (!this->WaitConsumed(lk))
         return;
   } while (this->IsHighWaterLevel(lk));
}

} // namespace
