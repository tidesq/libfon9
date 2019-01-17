/// \file fon9/io/FileIO.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/FileIO.hpp"
#include "fon9/io/DeviceRecvEvent.hpp"
#include "fon9/io/DeviceStartSend.hpp"
#include "fon9/TimedFileName.hpp"

namespace fon9 { namespace io {

#define fon9_FileIO_TAG_OutMode          "OutMode"
#define fon9_FileIO_TAG_InMode           "InMode"
#define fon9_FileIO_TAG_InSpeed          "InSpeed"
#define fon9_FileIO_TAG_InEOF            "InEOF"
#define fon9_FileIO_DEFAULT_InSize        0
#define fon9_FileIO_DEFAULT_InInterval    TimeInterval_Millisecond(1)

FileIO::InProps::InProps()
   : ReadSize_{fon9_FileIO_DEFAULT_InSize}
   , Interval_{fon9_FileIO_DEFAULT_InInterval} {
}
bool FileIO::InProps::Parse(const StrView tag, StrView value) {
   if (tag == "InStart") {
      this->StartPos_ = StrTo(value, this->StartPos_);
   } else if (tag == "InSize") {
      this->ReadSize_ = StrTo(value, this->ReadSize_);
   } else if (tag == "InInterval") {
      this->Interval_ = StrTo(value, this->Interval_);
   } else if (tag == fon9_FileIO_TAG_InSpeed) {
      const char* pend;
      this->ReadSize_ = StrTo(value, this->ReadSize_, &pend);
      value.SetBegin(pend);
      StrTrimHead(&value);
      if ((pend = value.Find('*')) != nullptr) {
         value.SetBegin(pend + 1);
         this->Interval_ = StrTo(value, this->Interval_);
      }
   } else if (tag == fon9_FileIO_TAG_InEOF) {
      if (value == "Broken")
         this->WhenEOF_ = WhenEOF::Broken;
      else if (value == "Close")
         this->WhenEOF_ = WhenEOF::Close;
      else if (value == "Dispose")
         this->WhenEOF_ = WhenEOF::Dispose;
      else if (isdigit(static_cast<unsigned char>(value.Get1st())))
         this->WhenEOF_ = static_cast<WhenEOF>(StrTo(value, 0u));
      else
         this->WhenEOF_ = WhenEOF::Dontcare;
   }
   else
      return false;

   if (this->Interval_.GetOrigValue() == 0)
      this->Interval_ = TimeInterval_Millisecond(1);
   return true;
}
void FileIO::InProps::AppendInfo(std::string& msg) {
   NumOutBuf outbuf;
   // |InSpeed=n*ti
   msg.append("|" fon9_FileIO_TAG_InSpeed "=");
   msg.append(ToStrRev(outbuf.end(), this->ReadSize_), outbuf.end());
   msg.push_back('*');
   msg.append(ToStrRev(outbuf.end(), this->Interval_), outbuf.end());
   // |InEOF=
   switch (this->WhenEOF_) {
   case WhenEOF::Dontcare: break;
   case WhenEOF::Broken:   msg.append("|" fon9_FileIO_TAG_InEOF "=Broken");  break;
   case WhenEOF::Close:    msg.append("|" fon9_FileIO_TAG_InEOF "=Close");   break;
   case WhenEOF::Dispose:  msg.append("|" fon9_FileIO_TAG_InEOF "=Dispose"); break;
   default:
      if (static_cast<int>(this->WhenEOF_) > 0) {
         msg.append("|" fon9_FileIO_TAG_InEOF "=");
         msg.append(ToStrRev(outbuf.end(), static_cast<unsigned>(this->WhenEOF_)), outbuf.end());
      }
      break;
   }
}
void FileIO::OpImpl_AppendDeviceInfo(std::string& info) {
   fon9_WARN_DISABLE_SWITCH;
   switch (this->OpImpl_GetState()) {
   case State::LinkReady:
      if (auto impl = this->ImplSP_.get()) {
         if (!impl->InFile_.IsOpened())
            info.push_back(impl->OutFile_.IsOpened() ? 'O' : '?');
         else {
            info.append(impl->OutFile_.IsOpened() ? "I+O" : "I");
            RevPrintAppendTo(info,
                             "|InCur=", impl->InCurPos_,
                             "|InFileTime=", impl->InFile_.GetLastModifyTime(),
                             "|InFileSize=", impl->InFile_.GetFileSize());
            if (impl->InSt_ == InFileSt::AtEOF)
               info.append("|AtEOF");
            else if (impl->InSt_ == InFileSt::ReadError)
               info.append("|ReadErr");
            this->InProps_.AppendInfo(info);
         }
      }
      break;
   }
   fon9_WARN_POP;
}
//--------------------------------------------------------------------------//
#ifndef fon9_HAVE_iovec
struct iovec {
   void*    iov_base;
   size_t   iov_len;
   inline friend void fon9_PutIoVectorElement(struct iovec* piov, void* dat, size_t datsz) {
      piov->iov_base = dat;
      piov->iov_len = datsz;
   }
};
#endif

void FileIO::Impl::OnTimer(TimeStamp now) {
   (void)now;
   iovec  blks[2];
   size_t expsz = this->RecvSize_ > RecvBufferSize::Default ? static_cast<size_t>(this->RecvSize_) : 1024;
   size_t blkc = this->InBuffer_.GetRecvBlockVector(blks, expsz);
   size_t rdsz = 0;
   iovec* pblk = blks;
   expsz = this->Owner_->InProps_.ReadSize_; // 每次讀入資料量.
   while (blkc > 0) {
      if (expsz > 0 && pblk->iov_len > expsz)
         pblk->iov_len = expsz;
      auto rdres = this->InFile_.Read(this->InCurPos_, pblk->iov_base, pblk->iov_len);
      if (rdres.IsError()) {
         this->InSt_ = InFileSt::ReadError;
         break;
      }
      this->InCurPos_ += rdres.GetResult();
      rdsz += rdres.GetResult();
      if (expsz > 0 && (expsz -= pblk->iov_len) <= 0)
         break;
      if (rdres.GetResult() != pblk->iov_len) {
         this->InSt_ = InFileSt::AtEOF;
         break;
      }
      ++pblk;
      --blkc;
   }
   DcQueueList& rxbuf = this->InBuffer_.SetDataReceived(rdsz);
   if (rdsz <= 0) { // AtEOF or ReadError
      this->InBuffer_.SetContinueRecv();
      this->CheckContinueRecv();
      return;
   }

   struct Aux {
      bool IsRecvBufferAlive(Device& dev, RecvBuffer& rbuf) const {
         return static_cast<FileIO*>(&dev)->ImplSP_.get() == &ContainerOf(rbuf, &Impl::InBuffer_);
      }
      void ContinueRecv(RecvBuffer& rbuf, RecvBufferSize expectSize, bool isReEnableReadable) const {
         (void)isReEnableReadable;
         Impl& impl = ContainerOf(rbuf, &Impl::InBuffer_);
         impl.RecvSize_ = expectSize;
         impl.CheckContinueRecv();
      }
      void DisableReadableEvent(RecvBuffer&) {
         // 只要不重啟 RunAfter(); 就不會有 Recv 事件, 所以這裡 do nothing.
      }
      SendDirectResult SendDirect(RecvDirectArgs& e, BufferList&& txbuf) {
         // SendDirect() 不考慮現在 impl.OutBuffer_ 的狀態?
         Impl&       impl = ContainerOf(RecvBuffer::StaticCast(e.RecvBuffer_), &Impl::InBuffer_);
         DcQueueList buf{std::move(txbuf)};
         impl.OutFile_.Append(buf);
         return SendDirectResult::Sent;
      }
   };
   Aux   aux;
   DeviceRecvBufferReady(*this->Owner_, rxbuf, aux);
}
void FileIO::Impl::CheckContinueRecv() {
   if (this->RecvSize_ < RecvBufferSize::Default || !this->InFile_.IsOpened())
      return;
   auto ti = this->Owner_->InProps_.Interval_;
   switch (this->InSt_) {
   default:
   case InFileSt::Readable:
      break;
   case InFileSt::ReadError:
      ti = TimeInterval_Second(1);
      break;
   case InFileSt::AtEOF:
      switch (auto whenEOF = this->Owner_->InProps_.WhenEOF_) {
      default:
         if (static_cast<int>(whenEOF) > 0)
            ti = TimeInterval_Second(static_cast<int>(whenEOF));
         break;
      case WhenEOF::Dontcare:
         break;
      case WhenEOF::Broken:
      case WhenEOF::Close:
      case WhenEOF::Dispose:
         fon9_WARN_DISABLE_PADDING;
         this->Owner_->OpQueue_.AddTask(DeviceAsyncOp{[this, whenEOF](Device& dev) {
            if (static_cast<FileIO*>(&dev)->ImplSP_.get() == this) {
               if (whenEOF == WhenEOF::Broken)
                  OpThr_SetBrokenState(*this->Owner_, "Broken when EOF.");
               else if (whenEOF == WhenEOF::Close)
                  OpThr_Close(*this->Owner_, "Close when EOF");
               else // if (whenEOF == WhenEOF::Dispose)
                  OpThr_Dispose(*this->Owner_, "Dispose when EOF");
            }
         }});
         fon9_WARN_POP;
         return;
      }
      break;
   }
   this->RunAfter(ti);
}
//--------------------------------------------------------------------------//
void FileIO::OpImpl_StartRecv(RecvBufferSize preallocSize) {
   assert(this->ImplSP_);
   this->ImplSP_->RecvSize_ = preallocSize;
   this->ImplSP_->CheckContinueRecv();
}
void FileIO::OpImpl_StopRunning() {
   if (auto impl = this->ImplSP_.get()) {
      impl->StopAndWait();
      this->ImplSP_.reset();
   }
}
void FileIO::OpImpl_Close(std::string cause) {
   this->OpImpl_StopRunning();
   this->OpImpl_SetState(State::Closed, &cause);
}
void FileIO::OpImpl_Reopen() {
   this->OpImpl_StopRunning();
   this->OpenImpl();
}
void FileIO::OpImpl_Open(const std::string cfgstr) {
   this->OpImpl_StopRunning();
   this->InFileCfg_.Mode_ = FileMode::CreatePath | FileMode::Read;
   this->OutFileCfg_.Mode_ = FileMode::CreatePath | FileMode::Append;

   StrView cfgpr{&cfgstr};
   StrView tag, value;
   while (StrFetchTagValue(cfgpr, tag, value)) {
      if (tag == "InFN")
         this->InFileCfg_.FileName_ = value.ToString();
      else if (tag == "OutFN")
         this->OutFileCfg_.FileName_ = value.ToString();
      else if (tag == fon9_FileIO_TAG_InMode)
         this->InFileCfg_.Mode_ = StrToFileMode(value) | FileMode::Read;
      else if (tag == fon9_FileIO_TAG_OutMode)
         this->OutFileCfg_.Mode_ = StrToFileMode(value) | FileMode::Append;
      else if (!this->InProps_.Parse(tag, value)) {
         // Unknown tag.
         this->OpImpl_SetState(State::ConfigError, ToStrView(RevPrintTo<std::string>(
            "Unknown '", tag, '=', value, '\''
            )));
         return;
      }
   }
   std::string devid;
   if (!this->InFileCfg_.FileName_.empty())
      devid = "I=" + this->InFileCfg_.FileName_;
   if (!this->OutFileCfg_.FileName_.empty()) {
      if (!devid.empty())
         devid.push_back('|');
      devid += "O=" + this->OutFileCfg_.FileName_;
   }
   OpThr_SetDeviceId(*this, std::move(devid));

   this->OpImpl_SetState(State::Opening, &cfgstr);
   this->OpenImpl();
}
File::Result FileIO::FileCfg::Open(File& fd, TimeStamp tm) const {
   TimedFileName fname(this->FileName_, TimedFileName::TimeScale::Day);
   fname.RebuildFileName(tm);
   return fd.Open(fname.GetFileName(), this->Mode_);
}
bool FileIO::OpenFile(StrView errMsgHead, const FileCfg& cfg, File& fd, TimeStamp tm) {
   if (cfg.FileName_.empty())
      return true;
   auto res = cfg.Open(fd, tm);
   if (!res.IsError())
      return true;
   this->OpImpl_SetState(State::LinkError, ToStrView(RevPrintTo<std::string>(
      "FileIO.Open.", errMsgHead, "|fn=", fd.GetOpenName(), '|', res
   )));
   return false;
}
void FileIO::OpenImpl() {
   assert(this->ImplSP_.get() == nullptr);
   if (this->OutFileCfg_.FileName_.empty() && this->InFileCfg_.FileName_.empty()) {
      this->OpImpl_SetState(State::LinkError, "FileIO.Open|err=None IO file names.");
      return;
   }
   TimeStamp now = UtcNow();
   ImplSP    impl{new Impl{*this}};
   if (this->OpenFile("OutFile", this->OutFileCfg_, impl->OutFile_, now)
       && this->OpenFile("InFile", this->InFileCfg_, impl->InFile_, now)) {
      this->ImplSP_ = std::move(impl);
      OpThr_SetLinkReady(*this, std::string{});
   }
}
ConfigParser::Result FileIO::OpImpl_SetProperty(StrView tag, StrView& value) {
   // 有需要在 LinkReady 狀態下, 設定屬性嗎?
   // 透過設定修改, 然後重建 device 應該就足夠了, 所以暫時不實作此 mf.
   return base::OpImpl_SetProperty(tag, value);
}
//--------------------------------------------------------------------------//
bool FileIO::IsSendBufferEmpty() const {
   bool res;
   this->OpQueue_.InplaceOrWait(AQueueTaskKind::Send, DeviceAsyncOp{[&res](Device& dev) {
      if (auto impl = static_cast<FileIO*>(&dev)->ImplSP_.get())
         res = impl->OutBuffer_.empty();
      else
         res = true;
   }});
   return res;
}
FileIO::SendResult FileIO::SendASAP(const void* src, size_t size) {
   // 為了簡化, FileIO::SendASAP() 一律採用 SendBuffered();
   // 因為一般而言使用 FileIO 不會有急迫性的寫入需求.
   return this->SendBuffered(src, size);
}
FileIO::SendResult FileIO::SendASAP(BufferList&& src) {
   return this->SendBuffered(std::move(src));
}
FileIO::SendResult FileIO::SendBuffered(const void* src, size_t size) {
   StartSendChecker sc;
   if (fon9_LIKELY(sc.IsLinkReady(*this))) {
      assert(this->ImplSP_);
      AppendToBuffer(this->ImplSP_->OutBuffer_, src, size);
      this->ImplSP_->AsyncFlushOutBuffer(sc.GetALocker());
      return Device::SendResult{0};
   }
   return Device::SendResult{std::errc::no_link};
}
FileIO::SendResult FileIO::SendBuffered(BufferList&& src) {
   StartSendChecker sc;
   if (fon9_LIKELY(sc.IsLinkReady(*this))) {
      assert(this->ImplSP_);
      this->ImplSP_->OutBuffer_.push_back(std::move(src));
      this->ImplSP_->AsyncFlushOutBuffer(sc.GetALocker());
      return Device::SendResult{0};
   }
   DcQueueList dcq{std::move(src)};
   dcq.ConsumeErr(std::errc::no_link);
   return Device::SendResult{std::errc::no_link};
}
void FileIO::Impl::AsyncFlushOutBuffer(DeviceOpQueue::ALockerForInplace& alocker) {
   ImplSP impl{this};
   alocker.AddAsyncTask(DeviceAsyncOp{[impl](Device&) {
      DcQueueList dcq{std::move(impl->OutBuffer_)};
      impl->OutFile_.Append(dcq);
   }});
}

} } // namespaces
