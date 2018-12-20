// \file fon9/fix/FixRecorder.cpp
// \author fonwinz@gmail.com
#include "fon9/fix/FixRecorder_Searcher.hpp"

namespace fon9 { namespace fix {

FixRecorder::~FixRecorder() {
   this->DisposeAsync();
   RevBufferList rbuf{256 + sizeof(NumOutBuf)};
   RevPrint(rbuf, f9fix_kCSTR_HdrInfo, UtcNow(), " f9fix.FixRecorder dtor.\n\n"); // 尾端多一個換行,可以比較容易區分重啟.
   auto lk{this->Worker_.Lock()};
   WorkContentController* app = static_cast<WorkContentController*>(&WorkContentController::StaticCast(*lk));
   app->AddWork(std::move(lk), rbuf.MoveOut());
   this->Worker_.TakeCallLocked(std::move(lk));
}

File::Result FixRecorder::Initialize(std::string fileName) {
   auto res = this->Open(std::move(fileName), FileMode::Append | FileMode::CreatePath | FileMode::Read | FileMode::DenyWrite).get();
   if (!res)
      return res;
   FixParser fixParser;
   fixParser.ResetExpectHeader(ToStrView(this->BeginHeader_));
   LastSeqSearcher   seqSearcher{fixParser};
   File&             file = this->GetStorage();
   res = seqSearcher.Start(file);
   if (!res)
      file.Close();
   else {
      if ((this->NextSendSeq_ = seqSearcher.NextSendSeq_) == 0)
         this->NextSendSeq_ = 1;
      if ((this->NextRecvSeq_ = seqSearcher.NextRecvSeq_) == 0)
         this->NextRecvSeq_ = 1;
      this->IdxInfoSizeInterval_ = 0;
      this->Write(f9fix_kCSTR_HdrInfo,
                  "f9fix.FixRecorder Initialized:"
                  "|NextSendSeq=", this->NextSendSeq_,
                  "|NextRecvSeq=", this->NextRecvSeq_);
   }
   return res;
}

void FixRecorder::WriteBuffer(Locker&& lk, RevBufferList&& rbuf) {
   BufferList wbuf = rbuf.MoveOut();
   if (fon9_UNLIKELY((this->IdxInfoSizeInterval_ += CalcDataSize(wbuf.cfront())) > kIdxInfoSizeInterval)) {
      this->IdxInfoSizeInterval_ = 0;
      RevPrint(rbuf, f9fix_kCSTR_HdrIdx
               f9fix_kCSTR_HdrNextSendSeq, this->NextSendSeq_,
               f9fix_kCSTR_HdrNextRecvSeq, this->NextRecvSeq_,
               '\n');
      wbuf.push_back(rbuf.MoveOut());
   }
   WorkContentController* app = static_cast<WorkContentController*>(&WorkContentController::StaticCast(*lk));
   app->AddWork(std::move(lk), std::move(wbuf));
}
void FixRecorder::WriteInputSeqReset(const StrView& fixmsg, FixSeqNum newSeqNo, bool isGapFill) {
   RevBufferList rbuf{static_cast<BufferNodeSize>(fixmsg.size() + 64)};
   RevPrint(rbuf,
            f9fix_kCSTR_HdrRecv, UtcNow(), ' ', fixmsg, '\n',
            isGapFill ? StrView{f9fix_kCSTR_HdrIdx} : StrView{f9fix_kCSTR_HdrRst},
            f9fix_kCSTR_HdrNextRecvSeq, newSeqNo, '\n');
   auto lk{this->Worker_.Lock()};
   this->NextRecvSeq_ = newSeqNo;
   this->WriteBuffer(std::move(lk), std::move(rbuf));
}
void FixRecorder::ForceResetRecvSeq(const StrView& info, FixSeqNum newSeqNo) {
   RevBufferList rbuf{static_cast<BufferNodeSize>(info.size() + 64)};
   RevPrint(rbuf,
            f9fix_kCSTR_HdrInfo, UtcNow(), ' ', info, '\n',
            f9fix_kCSTR_HdrRst f9fix_kCSTR_HdrNextRecvSeq, newSeqNo, '\n');
   auto lk{this->Worker_.Lock()};
   this->NextRecvSeq_ = newSeqNo;
   this->WriteBuffer(std::move(lk), std::move(rbuf));
}

} } // namespaces
