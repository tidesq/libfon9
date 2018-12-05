// \file fon9/fix/FixRecorder_Searcher.cpp
// \author fonwinz@gmail.com
#include "fon9/fix/FixRecorder_Searcher.hpp"

namespace fon9 { namespace fix {

// 當有回補要求時, 一律從檔案尾端往前尋找.
// - 因為可能會有 seq reset.
// - 索引訊息寫入時機:
//   - 不定時寫入: 大約輸出 n KB 時, 寫一次.
//   - 當有 seq reset 時寫入.
//   - ~FixRecorder() 關檔前.
// - 每次從尾端往前讀取 n KB, 然後用 memrchr() 尋找 f9fix_kCHAR_HdrCtrlMsgSeqNum.
//   - 直到找到所需的序號為止

//--------------------------------------------------------------------------//
// pbeg = 一行的開頭.
static const char* SkipTimestamp(const char* pbeg, const char* pend) {
   if (pend - pbeg < FixRecorder::kTimeStampWidth + kFixMinHeaderWidth)
      return nullptr;
   if (*++pbeg != ' ')
      return nullptr;
   if (*(pbeg += 1 + FixRecorder::kTimeStampWidth) != ' ')
      return nullptr;
   return pbeg + 1;
}
static inline FixSeqNum GetSeqNum(const char* pbeg, const char* pend, const char** endptr) {
   return StrTo(StrView{pbeg,pend}, static_cast<FixSeqNum>(0), endptr);
}
//--------------------------------------------------------------------------//
const char* FixRecorder::FixRevSercher::ParseControlMsgSeqNum(const char* pbeg, const char* pend,
                                                              FixSeqNum* outNextSend, FixSeqNum* outNextRecv) {
   if (static_cast<size_t>(pend - pbeg) < kControlMsgSeqNumMinLength)
      return nullptr;
   assert(*pbeg == f9fix_kCHAR_HdrCtrlMsgSeqNum);
   static_assert(f9fix_kCSTR_HdrIdx[0] == f9fix_kCHAR_HdrCtrlMsgSeqNum && sizeof(f9fix_kCSTR_HdrIdx) - 1 == 4, "Incorrect f9fix_kCSTR_HdrIdx[]?");
   static_assert(f9fix_kCSTR_HdrRst[0] == f9fix_kCHAR_HdrCtrlMsgSeqNum && sizeof(f9fix_kCSTR_HdrIdx) == sizeof(f9fix_kCSTR_HdrRst), "Incorrect kRstCstr[]?");

   #define CMP_CSTR_4(s,k) (s[0] == k[0] && s[1] == k[1] && s[2] == k[2] && s[3] == k[3])
   bool isIdx = CMP_CSTR_4(pbeg, f9fix_kCSTR_HdrIdx);
   bool isRst = !isIdx && CMP_CSTR_4(pbeg, f9fix_kCSTR_HdrRst);
   #undef CMP_CSTR_4

   if (!isIdx && !isRst)
      return nullptr;
   // 如果有 S,R 同時設定, 必定 S 在前: "|S=nnn|R=mmm"
   if (*(pbeg += sizeof(f9fix_kCSTR_HdrIdx) - 1) != f9fix_kCHAR_SPL)
      return nullptr;
   static_assert(f9fix_kCSTR_HdrNextSendSeq[0] == f9fix_kCHAR_SPL
                 && f9fix_kCSTR_HdrNextRecvSeq[0] == f9fix_kCHAR_SPL,
                 "Incorrect f9fix_kCSTR_HdrNextSendSeq[] or f9fix_kCSTR_HdrNextRecvSeq[]?");
   switch(*++pbeg) {
   case f9fix_kCSTR_HdrNextSendSeq[1]:
      if (*++pbeg != f9fix_kCSTR_HdrNextSendSeq[2])
         break;
      ++pbeg;
      if (isRst)
         this->RstFlags_ = static_cast<RstFlags>(this->RstFlags_ | RstSend);
      if (outNextSend && *outNextSend <= 0) {
         *outNextSend = GetSeqNum(pbeg, pend, &pbeg);
         if (static_cast<size_t>(pend - pbeg) < sizeof(f9fix_kCSTR_HdrNextRecvSeq))
            break; // 必須有足夠空間, 用來確保底下的 *pbeg 判斷.
         if (*pbeg != f9fix_kCHAR_SPL)
            break;
      }
      else {
         if ((pbeg = static_cast<const char*>(memchr(pbeg, f9fix_kCHAR_SPL, static_cast<size_t>(pend - pbeg)))) == nullptr)
            return nullptr;
         if (static_cast<size_t>(pend - pbeg) < sizeof(f9fix_kCSTR_HdrNextRecvSeq))
            break;
      }
      if (*++pbeg != f9fix_kCSTR_HdrNextRecvSeq[1])
         break;
      // 有設定 'R', 所以不用 break; 繼續下去.
   case f9fix_kCSTR_HdrNextRecvSeq[1]:
      if (*++pbeg != f9fix_kCSTR_HdrNextRecvSeq[2])
         break;
      ++pbeg;
      if (isRst)
         this->RstFlags_ = static_cast<RstFlags>(this->RstFlags_ | RstRecv);
      if (outNextRecv && *outNextRecv <= 0)
         *outNextRecv = GetSeqNum(pbeg, pend, &pbeg);
      break;
   }
   return pbeg;
}
const char* FixRecorder::FixRevSercher::ParseFixLine_MsgSeqNum(const char* pbeg, const char* pend, FixSeqNum& seqn) {
   if (seqn > 0 || (pbeg = SkipTimestamp(pbeg, pend)) == nullptr)
      return nullptr;
   StrView fixmsg{pbeg,pend};
   this->FixParser_.Clear();
   this->FixParser_.ParseFields(fixmsg, FixParser::Until::MsgSeqNum);
   seqn = this->FixParser_.GetMsgSeqNum();
   return pbeg;
}
//--------------------------------------------------------------------------//
LoopControl FixRecorder::LastSeqSearcher::OnFileBlock(size_t rdsz) {
   return this->RevSearchBlock(this->GetBlockPos(), '\n', rdsz);
   //不用理會檔頭第一行訊息, 因為該訊息為 FixRecorder 的開始資訊. 不是 FIX Message.
   //if (this->GetBlockPos() == 0)
   //   CheckLine(this->GetBlockBuffer(), this->DataEnd_);
   //return true;
}
void FixRecorder::LastSeqSearcher::ParseFixLine_MsgSeqNum_ToNextSeq(const char* pbeg, const char* pend, FixSeqNum& seqn) {
   if (this->ParseFixLine_MsgSeqNum(pbeg, pend, seqn))
      if (seqn > 0)
         ++seqn;
}
LoopControl FixRecorder::LastSeqSearcher::OnFoundChar(char* pbeg, char* pend) {
   assert(pbeg < pend);
   switch (*++pbeg) {
   default: return LoopControl::Continue;
   case f9fix_kCSTR_HdrRecv[0]:
      this->ParseFixLine_MsgSeqNum_ToNextSeq(pbeg, pend, this->NextRecvSeq_);
      break;
   case f9fix_kCSTR_HdrSend[0]:
      this->ParseFixLine_MsgSeqNum_ToNextSeq(pbeg, pend, this->NextSendSeq_);
      break;
   case f9fix_kCHAR_HdrCtrlMsgSeqNum:
      this->ParseControlMsgSeqNum(pbeg, pend, &this->NextSendSeq_, &this->NextRecvSeq_);
      break;
   }
   return (this->NextRecvSeq_ <= 0 || this->NextSendSeq_ <= 0) ? LoopControl::Continue : LoopControl::Break;
}
//--------------------------------------------------------------------------//
File::Result FixRecorder::SentMessageSearcher::Start(ReloadSent& reloader, FixSeqNum expectSeq, File& fd) {
   File::Result res = fd.GetFileSize();
   if (!res)
      return res;
   this->PosAtBefore_ = this->PosAtAfter_ = 0;
   this->ExpectSeq_ = expectSeq;
   this->FoundLine_.Reset(nullptr);
   return base::Start(fd, reloader.Buffer_, kReloadSentBufferSize);
}
LoopControl FixRecorder::SentMessageSearcher::OnFileBlock(size_t rdsz) {
   if (this->PosAtBefore_ == 0)
      this->PosAtBefore_ = this->GetBlockPos() + rdsz;
   this->PtrAtBefore_ = this->PtrAtAfter_ = nullptr;
   this->RevSearchBlock(this->GetBlockPos(), f9fix_kCHAR_HdrCtrlMsgSeqNum, rdsz);
   // 如果透過 idx 找到 seq: 還是需要往後找到「正確序號的 FIX Message line」.
   // 如果沒找到 idx: 則尋找讀入的區塊 "\n" "S ".
   const char* const pend = (this->PtrAtBefore_ ? this->PtrAtBefore_ : (this->BlockBufferPtr_ + rdsz));
   const char* pbeg = (this->PtrAtAfter_ ? this->PtrAtAfter_ : this->BlockBufferPtr_);
   if ((pbeg = reinterpret_cast<const char*>(memchr(pbeg, '\n', static_cast<size_t>(pend - pbeg)))) != nullptr) {
      while (pend - (++pbeg) > kFixMinMessageWidth) {
         const char* lnNext = reinterpret_cast<const char*>(memchr(pbeg, '\n', static_cast<size_t>(pend - pbeg)));
         if (lnNext == nullptr)
            break;
         if (*pbeg == f9fix_kCSTR_HdrSend[0]) {
            FixSeqNum msgSeqNum = 0;
            pbeg = this->ParseFixLine_MsgSeqNum(pbeg, lnNext, msgSeqNum);
            if (pbeg && msgSeqNum > 0) {
               if (this->ExpectSeq_ < msgSeqNum) {
                  if (this->PtrAtAfter_)
                     // 有設定 AtAfter, 但要找的訊息卻在pbeg之前:
                     // 表示找不到相符的, 但找對最接近的.
                     goto __FOUND_LINE;
                  this->PtrAtBefore_ = pbeg;
                  this->PosAtBefore_ = this->GetBlockPos() + (pbeg - this->BlockBufferPtr_);
                  return LoopControl::Continue;
               }
               if (msgSeqNum == this->ExpectSeq_) {
               __FOUND_LINE:
                  this->FoundLine_.Reset(pbeg, lnNext);
                  if (this->DataEnd_ > this->BlockBufferPtr_ + this->BlockSize_)
                     // 因為 FileRevSearch::RevSearchBlock() 會:「把剩下沒處理的資料,放到緩衝區尾端. 下次讀入 block 之後接續處理.」
                     // 所以如果有找到, 則超過 this->BlockBufferPtr_ + this->BlockSize_ 的資料時無效的!
                     this->DataEnd_ = this->BlockBufferPtr_ + this->BlockSize_;
                  goto __RETURN_BREAK_READ_LOOP;
               }
               // 此行的 msgSeqNum < ExpectSeq: 繼續往後找.
               this->SetAtAfter(lnNext);
            }
         }
         pbeg = lnNext;
      }
   }
   if (this->PtrAtAfter_ == nullptr)
      return LoopControl::Continue;
__RETURN_BREAK_READ_LOOP:
   if (this->DataEnd_ < pend)
      this->DataEnd_ = const_cast<char*>(pend);
   return LoopControl::Break;
}
LoopControl FixRecorder::SentMessageSearcher::OnFoundChar(char* pbeg, char* pend) {
   assert(*pbeg == f9fix_kCHAR_HdrCtrlMsgSeqNum);
   FixSeqNum nextSendSeq = 0;
   pend = const_cast<char*>(this->ParseControlMsgSeqNum(pbeg, pend, &nextSendSeq, nullptr));
   if (nextSendSeq <= 0)
      return LoopControl::Continue;
   if (nextSendSeq <= this->ExpectSeq_ || (this->RstFlags_ & RstSend) != 0) {
      this->SetAtAfter(pend);
      return LoopControl::Break;
   }
   //(this->ExpectSeq_ < nextSendSeq)
   // 因為此行這裡只是 MsgSeqNum 的訊息, 不是真正的 "S timestamp FIX Message",
   // 所以不用設定 this->PosAtBefore_;
   // 這樣才能在找不到 ExpectSeq_ 時, 繼續往 pbeg 之後找到最接近的
   this->PtrAtBefore_ = pbeg;
   return LoopControl::Continue;
}
//--------------------------------------------------------------------------//
bool FixRecorder::ReloadSent::InitStart(FixRecorder& fixRecorder, FixSeqNum seqFrom) {
   this->FixParser_.ResetExpectHeader(ToStrView(fixRecorder.BeginHeader_));
   this->CurBufferPos_ = 0;
   this->CurMsg_.Reset(nullptr);
   if (seqFrom <= 0 || fixRecorder.NextSendSeq_ <= seqFrom) // 未送出的訊息序號, 必定找不到!
      return false;
   fixRecorder.WaitFlushed();
   return true;
}
StrView FixRecorder::ReloadSent::Find(FixRecorder& fixRecorder, FixSeqNum seq) {
   if (!this->InitStart(fixRecorder, seq))
      return StrView{};
   SentMessageSearcher  searcher{*this};
   File::Result         res = searcher.Start(*this, seq, fixRecorder.GetStorage());
   if (fon9_LIKELY(res && !searcher.FoundLine_.empty())) {
      if (searcher.FixParser_.GetMsgSeqNum() == seq)
         return searcher.FoundLine_;
   }
   return StrView{};
}
StrView FixRecorder::ReloadSent::Start(FixRecorder& fixRecorder, FixSeqNum seqFrom) {
   if (!this->InitStart(fixRecorder, seqFrom))
      return StrView{};
   SentMessageSearcher  searcher{*this};
   File::Result         res = searcher.Start(*this, seqFrom, fixRecorder.GetStorage());
   if (!res)
      return StrView{};
   this->DataEnd_ = searcher.DataEnd_;
   if (fon9_LIKELY(!searcher.FoundLine_.empty())) {
      this->CurBufferPos_ = searcher.GetBlockPos();
      this->CurMsg_ = searcher.FoundLine_;
      fixRecorder.Write(f9fix_kCSTR_HdrInfo,
                        "ReloadSent:"
                        "|seq=", seqFrom,
                        "|foundAt=", searcher.GetBlockPos() + searcher.FoundLine_.begin() - this->Buffer_,
                        "|foundSeq=", searcher.FixParser_.GetMsgSeqNum());
      return this->CurMsg_;
   }
   StrView errmsg;
   if (searcher.PosAtAfter_ >= searcher.PosAtBefore_) // 檔案有問題?!
      errmsg = StrView{"|err=Invalid pos."};
   else {
      //從 searcher.PosAtAfter_(可能為0) 開始往檔尾找, 直到 searcher.PosAtBefore_
      //如果沒找到, 則 this->PosAtBefore_ 視為開始位置(讀取該筆 "S timestamp FIX Message").
      this->CurBufferPos_ = searcher.GetBlockPos();
      if (searcher.PtrAtAfter_ == nullptr)
         searcher.PtrAtAfter_ = this->Buffer_;
      this->CurMsg_.Reset(searcher.PtrAtAfter_, searcher.PtrAtAfter_);
      if (this->FindNext(fixRecorder).empty())
         errmsg = StrView{"|err=Not found."};
      else {
         StrView  fixmsg{this->CurMsg_};
         this->FixParser_.Clear();
         this->FixParser_.ParseFields(fixmsg, FixParser::Until::MsgSeqNum);
         fixRecorder.Write(f9fix_kCSTR_HdrInfo,
                           "ReloadSent:"
                           "|seq=", seqFrom,
                           "|atAfter=", searcher.PosAtAfter_,
                           "|atBefore=", searcher.PosAtBefore_,
                           "|foundAt=", this->CurBufferPos_ + this->CurMsg_.begin() - this->Buffer_,
                           "|foundSeq=", this->FixParser_.GetMsgSeqNum());
         return this->CurMsg_;
      }
   }
   if (!errmsg.empty())
      fixRecorder.Write(f9fix_kCSTR_HdrInfo,
                        "ReloadSent:"
                        "|seq=", seqFrom,
                        "|atAfter=", searcher.PosAtAfter_,
                        "|atBefore=", searcher.PosAtBefore_,
                        errmsg);
   return this->CurMsg_ = nullptr;
}
StrView FixRecorder::ReloadSent::FindNext(FixRecorder& fixRecorder) {
   if (this->CurMsg_.begin() == nullptr)
      return this->CurMsg_;
   const char* pbeg = this->CurMsg_.end();
   size_t      remainSize = static_cast<size_t>(this->DataEnd_ - pbeg);
   assert(remainSize <= kReloadSentBufferSize);
   for (;;) {
      if (remainSize > 0) {
         if (*pbeg == '\n') {
            ++pbeg;
            --remainSize;
         }
         while (remainSize > 0) {
            const char* pend = reinterpret_cast<const char*>(memchr(pbeg, '\n', remainSize));
            if (pend == nullptr) {
               if (remainSize > kMaxFixMsgBufferSize)
                  remainSize = kMaxFixMsgBufferSize;
               memcpy(this->Buffer_, pbeg, remainSize);
               break;
            }
            if (*pbeg == 'S') {
               if ((pbeg = SkipTimestamp(pbeg, pend)) != nullptr)
                  return this->CurMsg_ = StrView{pbeg, pend};
            }
            remainSize = static_cast<size_t>(this->DataEnd_ - (pbeg = pend + 1));
         }
      }
      // 載入下一個區塊.
      fixRecorder.WaitFlushed();
      this->CurBufferPos_ += static_cast<size_t>(this->DataEnd_ - this->Buffer_);
      File::Result res = fixRecorder.GetStorage().Read(this->CurBufferPos_, this->Buffer_ + remainSize, kReloadSentBufferSize / 2);
      if (!res)
         return this->CurMsg_ = nullptr;
      this->CurBufferPos_ -= remainSize;
      pbeg = this->Buffer_;
      this->DataEnd_ = this->Buffer_ + (remainSize += res.GetResult());
      if (res.GetResult() == 0)
         return this->CurMsg_ = StrView{pbeg, pbeg};
   }
}
bool FixRecorder::ReloadSent::IsErrOrEOF(FixRecorder& fixRecorder, FixRecorder::Locker& locker) const {
   if (this->CurMsg_.empty())
      return true;
   fixRecorder.WaitFlushed(locker);
   File::Result res = fixRecorder.GetStorage().GetFileSize();
   if (!res)
      return true;
   return (this->CurBufferPos_ + (this->DataEnd_ - this->Buffer_)) >= res.GetResult();
}

} } // namespaces
