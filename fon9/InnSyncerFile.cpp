/// \file fon9/InnSyncerFile.cpp
/// \author fonwinz@gmail.com
#include "fon9/InnSyncerFile.hpp"
#include "fon9/File.hpp"
#include "fon9/FilePath.hpp"
#include "fon9/Log.hpp"
#include "fon9/Timer.hpp"
#include "fon9/Endian.hpp"
#include "fon9/buffer/DcQueueList.hpp"
#include "fon9/buffer/FwdBufferList.hpp"

namespace fon9 {

// 實際記錄的資料: 0xff 0xff 0xff 0xff + ExHeaderSizeT pksz(BigEndian) + packet.
using ExHeaderSizeT = uint64_t;
static const char    kExHeaderMessage[4] = {'\xff', '\xff', '\xff', '\xff'};
static const size_t  kExHeaderSize = sizeof(kExHeaderMessage) + sizeof(ExHeaderSizeT);

class InnSyncerFile::Impl {
   fon9_NON_COPY_NON_MOVE(Impl);
   // 匯入檔: 集中合併後, 再送出來:
   // - 這樣可以降低同步檔案處理的複雜度.
   File  SynOut_;
   File  SynIn_;
   struct SynInTimer : public DataMemberTimer {
      fon9_NON_COPY_NON_MOVE(SynInTimer);
      virtual void EmitOnTimer(TimeStamp now) override;
      SynInTimer() = default;
   };
   SynInTimer     SynInTimer_;
   File::PosType  SynInPos_{0};
   InnSyncerFile& Owner_;
   File::PosType  SearchingExHeaderFrom_{0};
   TimeInterval   SyncInInterval_;

   static bool OpenFile(File& fd, StrView fname, FileMode fmode) {
      Result res = fd.Open(fname.ToString(), fmode);
      if (res)
         return true;
      fon9_LOG_ERROR("InnSyncerFile.ctor|fname=", fname, "|err=", res);
      return false;
   }

public:
   using Result = File::Result;

   Impl(InnSyncerFile& owner, const CreateArgs& args)
      : Owner_(owner)
      , SyncInInterval_{args.SyncInInterval_} {
      if (!OpenFile(this->SynOut_, &args.SyncOutFileName_, FileMode::CreatePath | FileMode::Append | FileMode::DenyWrite)
       || !OpenFile(this->SynIn_,  &args.SyncInFileName_,  FileMode::CreatePath | FileMode::Read)) {
         args.Result_ = owner.State_ = InnSyncer::State::ErrorCtor;
         this->SynOut_.Close();
         this->SynIn_.Close();
         return;
      }
      args.Result_ = owner.State_ = State::Ready;
      // SynIn_ 每次都從頭讀起(SynInPos_=0)? 可考慮把 SynInPos_ 保留起來!
      // => 反正有 SyncKey 可以判斷, 所以從頭讀起亦無不可, 只是如果資料量大時, 會多花些時間.
   }
   ~Impl() {
      this->SynInTimer_.DisposeAndWait();
   }
   void StartSync() {
      if (this->Owner_.State_ != State::Ready)
         return;
      this->Owner_.State_ = State::Running;
      this->SynInTimer_.RunAfter(this->SyncInInterval_);
   }
   void StopSync() {
      if (this->Owner_.State_ != State::Running)
         return;
      this->Owner_.State_ = State::Stopping;
      this->SynInTimer_.StopAndWait();
      this->Owner_.State_ = State::Stopped;
   }
   void WriteSyncImpl(RevBufferList&& rbuf) {
      ExHeaderSizeT  pksz = CalcDataSize(rbuf.cfront());
      char*          pout = rbuf.AllocPrefix(kExHeaderSize);
      PutBigEndian(pout - sizeof(pksz), pksz);
      memcpy(pout -= kExHeaderSize, kExHeaderMessage, sizeof(kExHeaderMessage));
      rbuf.SetPrefixUsed(pout);

      DcQueueList outbuf{rbuf.MoveOut()};
      auto        res = this->SynOut_.Append(outbuf);
      if (!res || res.GetResult() != pksz + kExHeaderSize) {
         fon9_LOG_ERROR("InnSyncerFile.Write|fname=", this->SynOut_.GetOpenName(),
                        "|err=", res, "|expect=", pksz + kExHeaderSize);
      }
   }
};

void InnSyncerFile::Impl::SynInTimer::EmitOnTimer(TimeStamp now) {
   (void)now;
   InnSyncerFile::Impl& impl = ContainerOf(*this, &Impl::SynInTimer_);
   for (;;) {
      const File::PosType curpos = impl.SynInPos_;
      char exHeader[kExHeaderSize];
      auto res = impl.SynIn_.Read(curpos, exHeader, sizeof(exHeader));
      if (!res || res.GetResult() != sizeof(exHeader))
         break;
      if (memcmp(exHeader, kExHeaderMessage, sizeof(kExHeaderMessage)) != 0) {
         // header error.
         if (impl.SearchingExHeaderFrom_ == 0) {
            impl.SearchingExHeaderFrom_ = curpos + 1;
            fon9_LOG_ERROR("InnSyncerFile.Read|fname=", impl.SynIn_.GetOpenName(),
                           "|pos=", curpos, "|err=unknown header");
         }
         if (void* pofs = memchr(exHeader, kExHeaderMessage[0], sizeof(exHeader))) {
            if (pofs == exHeader)
               ++impl.SynInPos_;
            else
               impl.SynInPos_ += (reinterpret_cast<char*>(pofs) - exHeader);
         }
         else
            impl.SynInPos_ += sizeof(exHeader);
         continue;
      }
      ExHeaderSizeT  pksz = GetBigEndian<ExHeaderSizeT>(exHeader + sizeof(kExHeaderMessage));
      if (impl.SearchingExHeaderFrom_) {
         fon9_LOG_ERROR("InnSyncerFile.Read|fname=", impl.SynIn_.GetOpenName(),
                        "|dropFrom=", impl.SearchingExHeaderFrom_ - 1,
                        "|dropTo=", curpos,
                        "|nextPksz=", pksz);
         impl.SearchingExHeaderFrom_ = 0;
      }
      FwdBufferNode* bufNode;
      BufferList     buf;
      buf.push_back(bufNode = FwdBufferNode::Alloc(pksz));
      res = impl.SynIn_.Read(curpos + sizeof(exHeader), bufNode->GetDataEnd(), pksz);
      if (!res) {
         fon9_LOG_ERROR("InnSyncerFile.Read|fname=", impl.SynIn_.GetOpenName(),
                        "|pos=", curpos + sizeof(exHeader),
                        "|read=", pksz,
                        "|err=", res);
         break;
      }
      if (res.GetResult() != pksz)
         break;
      impl.SynInPos_ += sizeof(exHeader) + pksz;
      bufNode->SetDataEnd(bufNode->GetDataEnd() + pksz);

      try {
         DcQueueList dcbuf{std::move(buf)};
         if (impl.Owner_.OnInnSyncRecv(dcbuf) <= 0) {
            fon9_LOG_ERROR("InnSyncerFile.Read|fname=", impl.SynIn_.GetOpenName(),
                           "|pos=", curpos,
                           "|err=unknown sync data.");
         }
      }
      catch (std::exception& e) {
         fon9_LOG_ERROR("InnSyncerFile.Read|fname=", impl.SynIn_.GetOpenName(),
                        "|pos=", curpos,
                        "|syncErr=", e.what());
      }
   }
   this->RunAfter(impl.SyncInInterval_);
}

//--------------------------------------------------------------------------//

fon9_MSC_WARN_DISABLE(4355); // 'this': used in base member initializer list
InnSyncerFile::InnSyncerFile(const CreateArgs& args)
   : Impl_{new Impl{*this, args}} {
}
fon9_MSC_WARN_POP;

InnSyncerFile::~InnSyncerFile() {
   this->Impl_->StopSync();
}

InnSyncerFile::State InnSyncerFile::StartSync() {
   this->Impl_->StartSync();
   return this->State_;
}
void InnSyncerFile::StopSync() {
   this->Impl_->StopSync();
}

void InnSyncerFile::WriteSyncImpl(RevBufferList&& rbuf) {
   return this->Impl_->WriteSyncImpl(std::move(rbuf));
}

} // namespaces
