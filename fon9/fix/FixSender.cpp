// \file fon9/fix/FixSender.cpp
// \author fonwinz@gmail.com
#include "fon9/fix/FixSender.hpp"
#include "fon9/fix/FixRecorder_Searcher.hpp"
#include "fon9/fix/FixAdminDef.hpp"
#include "fon9/fix/FixConfig.hpp"
#include "fon9/FwdPrint.hpp"

namespace fon9 { namespace fix {

FixSender::~FixSender() {
}
void FixSender::Send(Locker&&       locker,
                     StrView        fldMsgType,
                     FixBuilder&&   fixmsgBuilder,
                     FixSeqNum      nextSeqNum,
                     RevBufferList* fixmsgDupOut) {
   // CompIDs
   RevBuffer& msgRBuf = fixmsgBuilder.GetBuffer();
   RevPrint(msgRBuf, this->CompIDs_.Header_);
   // SendingTime.
   fixmsgBuilder.PutUtcNow();
   TimeStamp now = this->LastSentTime_ = fixmsgBuilder.GetUtcNow();
   RevPrint(msgRBuf, f9fix_SPLTAGEQ(SendingTime));

   // MsgType: ** ALWAYS THIRD FIELD IN MESSAGE. (Always unencrypted) **
   // 底下的欄位順序不可改變, 因為 Replayer::Rebuild() 依賴此順序重建要 replay 的訊息.
   // BeginString|9=BodyLength|35=MsgType|34=MsgSeqNum|52=SendingTime
   //               \________/                            \_ replay時插入額外欄位.
   //
   FixSeqNum msgSeqNum = this->GetNextSendSeq(locker);
   RevPrint(msgRBuf, fldMsgType, f9fix_SPLTAGEQ(MsgSeqNum), msgSeqNum);

   // 產出 FIX Message.
   BufferList  fixmsg{fixmsgBuilder.Final(ToStrView(this->BeginHeader_))};
   const auto  fixmsgSize = CalcDataSize(fixmsg.cfront());
   if (fixmsgDupOut)
      RevPrint(*fixmsgDupOut, fixmsg);
   // 建立要寫入 FixRecorder 的訊息.
   RevBufferList rlog{static_cast<BufferNodeSize>(64 + fixmsgSize)};
   if (fon9_LIKELY(nextSeqNum == 0))
      nextSeqNum = msgSeqNum + 1;
   else
      RevPrint(rlog, f9fix_kCSTR_HdrRst f9fix_kCSTR_HdrNextSendSeq, nextSeqNum, '\n');
   RevPrint(rlog, f9fix_kCSTR_HdrSend, now, ' ', fixmsg, '\n');
   // 送出訊息.
   if (fon9_LIKELY(!this->IsReplayingAll_))
      this->OnSendFixMessage(locker, std::move(fixmsg));
   else
      DcQueueList{std::move(fixmsg)}.PopConsumed(fixmsgSize);
   this->WriteAfterSend(std::move(locker), std::move(rlog), nextSeqNum);
}

void FixSender::ResetNextSendSeq(FixSeqNum nextSeqNum) {
   if (nextSeqNum <= 0)
      return;
   RevBufferList rlog{128};
   RevPrint(rlog, f9fix_kCSTR_HdrRst f9fix_kCSTR_HdrNextSendSeq, nextSeqNum, '\n');
   this->WriteAfterSend(this->Lock(), std::move(rlog), nextSeqNum);
}
void FixSender::SequenceReset(FixSeqNum newSeqNo) {
   FixBuilder msgSequenceReset;
   RevPrint(msgSequenceReset.GetBuffer(), f9fix_SPLTAGEQ(NewSeqNo), newSeqNo);
   this->Send(this->Lock(), f9fix_SPLFLDMSGTYPE(SequenceReset), std::move(msgSequenceReset), newSeqNo, nullptr);
}

//--------------------------------------------------------------------------//

// 找到舊訊息的:
// - MsgType: 決定此筆訊息是否需要 Replay
// - MsgSeqNum:
//    - 是否需要 Sequence Reset-GapFill?
//    - 是否已到所要求的最後一筆?
// - SendingTime 的位置:
//   然後將 "SendingTime" 改成: "SendingTime=now|PossDupFlag=Y|OrigSendTime"
//   "|52=" "XXX" => "|52=" "now|43=Y|122=" "XXX"
//           ^^^             \__ReFlds___/  直接插入即可, 其餘訊息完全不須變動, 只需重算 BodyLength 及 CheckSum.
//
fon9_WARN_DISABLE_PADDING;
struct FixSender::Replayer {
   fon9_NON_COPY_NON_MOVE(Replayer);

   #define REFLDS_kCSTR_TIME  "yyyymmdd-hh:mm:ss.nnn"
   #define REFLDS_kCSTR       REFLDS_kCSTR_TIME f9fix_SPLTAGEQ(PossDupFlag) "Y" f9fix_SPLTAGEQ(OrigSendingTime)
   static_assert(sizeof(REFLDS_kCSTR_TIME) - 1 == kDateTimeStrWidth_FIXMS, "error #define REFLDS_kCSTR_TIME");
   enum {
      kReFldsWidth = sizeof(REFLDS_kCSTR) - 1
   };
   char              ReFlds_[kReFldsWidth];
   StrView           CurMsg_;
   FwdBufferList     SendBuf_{512};
   FixRecorder&      FixRecorder_;
   const FixConfig*  FixConfig_;
   FixSeqNum         BeginSeqNo_;
   const FixSeqNum   EndSeqNo_;

   Replayer(FixRecorder& fixRecorder, const FixConfig* fixConfig, FixSeqNum beginSeqNo, FixSeqNum endSeqNo)
      : FixRecorder_(fixRecorder)
      , FixConfig_(fixConfig)
      , BeginSeqNo_{beginSeqNo}
      , EndSeqNo_{endSeqNo} {
      memcpy(this->ReFlds_ + kDateTimeStrWidth_FIXMS, REFLDS_kCSTR + kDateTimeStrWidth_FIXMS, kReFldsWidth - kDateTimeStrWidth_FIXMS);
      ToStrRev_FIXMS(ReFlds_ + kDateTimeStrWidth_FIXMS, UtcNow());
   }
   #undef REFLDS_kCSTR_TIME
   #undef REFLDS_kCSTR

   bool Rebuild(const FixParser::FixField* fldMsgType, const FixParser::FixField* fldSendingTime, FwdBufferList& logbuf) {
      const char* const pfldMsgType = fldMsgType->Value_.begin() - (sizeof(f9fix_SPLTAGEQ(MsgType)) - 1);
      const char*       pvalSendingTime = fldSendingTime->Value_.begin();
      // 依照規定 MsgType 為第3個欄位, 此判斷必定為 true, 但避免因 Recorder 的內容損毀而 crash, 所以仍加上判斷.
      assert(pfldMsgType < pvalSendingTime);
      if (fon9_UNLIKELY(pfldMsgType >= pvalSendingTime))
         return false;

      const char* const pendCurMsg = this->CurMsg_.end();
      byte* const       pbegNode = reinterpret_cast<byte*>(logbuf.AllocSuffix(this->CurMsg_.size() + kReFldsWidth));
      byte*             pout = pbegNode;
      pout = PutFwd(pout, this->FixRecorder_.BeginHeader_.begin(), this->FixRecorder_.BeginHeader_.size());

      NumOutBuf    numbuf;
      const size_t bodyLength = static_cast<size_t>(pendCurMsg - pfldMsgType - kFixTailWidth + kReFldsWidth);
      pout = PutFwd(pout, ToStrRev(numbuf.end(), bodyLength), numbuf.end());
      pout = PutFwd(pout, pfldMsgType, fldSendingTime->Value_.begin());
      pout = PutFwd(pout, this->ReFlds_, kReFldsWidth);
      pout = PutFwd(pout, pvalSendingTime, pendCurMsg - kFixTailWidth);
      // 重算 CheckSum.
      byte* pbegCKS = pbegNode;
      byte  cks = f9fix_kCHAR_SPL;
      while (pbegCKS < pout)
         cks = static_cast<byte>(cks + *pbegCKS++);
      FixBuilder::PutCheckSumField(reinterpret_cast<char*>(pbegCKS), cks);
      *(pout += kFixTailWidth) = '\n';
      // 填入準備傳送的 FIX Message buffer.
      FwdPutMem(this->SendBuf_, pbegNode, pout);
      // 填入要寫入 Recorder 的 buffer.
      logbuf.SetSuffixUsed(reinterpret_cast<char*>(pout + 1));
      return true;
   }
   void SeqReset(bool isGapFill, FixSeqNum oldSeqNo, FixSeqNum newSeqNo, FwdBufferList& logbuf) {
      FixBuilder  msgSequenceReset;
      auto&       rbuf = msgSequenceReset.GetBuffer();
      if (isGapFill)
         RevPrint(rbuf, f9fix_SPLTAGEQ(GapFillFlag) "Y");
      RevPrint(rbuf, f9fix_SPLTAGEQ(NewSeqNo), newSeqNo);
      // ----- header -----
      RevPrint(rbuf, this->FixRecorder_.CompIDs_.Header_);
      RevPutMem(rbuf, this->ReFlds_, kDateTimeStrWidth_FIXMS);
      RevPrint(rbuf, f9fix_SPLTAGEQ(SendingTime));
      RevPutMem(rbuf, this->ReFlds_, kDateTimeStrWidth_FIXMS);
      RevPrint(rbuf, f9fix_SPLTAGEQ(PossDupFlag) "Y" f9fix_SPLTAGEQ(OrigSendingTime));
      RevPrint(rbuf, f9fix_SPLFLDMSGTYPE(SequenceReset) f9fix_SPLTAGEQ(MsgSeqNum), oldSeqNo);

      BufferList  buf{msgSequenceReset.Final(ToStrView(this->FixRecorder_.BeginHeader_))};
      FwdPrint(this->SendBuf_, buf);
      logbuf.PushBack(std::move(buf));
      FwdPrint(logbuf, '\n');
   }
   void SeqReset(FixSeqNum oldSeqNo, FixSeqNum newSeqNo, FwdBufferList& logbuf) {
      this->SeqReset(false, oldSeqNo, newSeqNo, logbuf);
   }
   void GapFill(FixSeqNum oldSeqNo, FixSeqNum newSeqNo, FwdBufferList& logbuf) {
      this->SeqReset(true, oldSeqNo, newSeqNo, logbuf);
   }

   void Reload(FixRecorder::ReloadSent& reloader, FwdBufferList& logbuf) {
      while (!this->CurMsg_.empty()) {
         reloader.FixParser_.Clear();
         StrView           strpr{this->CurMsg_};
         FixParser::Result res = reloader.FixParser_.ParseFields(strpr, FixParser::Until::MsgType | FixParser::Until::MsgSeqNum | FixParser::Until::SendingTime);
         if (res > FixParser::NeedsMore) {
            const FixSeqNum curSeqNo = reloader.FixParser_.GetMsgSeqNum();
            if (this->EndSeqNo_ != 0 && this->EndSeqNo_ < curSeqNo)
               break;
            const FixParser::FixField* fldMsgType = reloader.FixParser_.GetField(f9fix_kTAG_MsgType);
            const FixParser::FixField* fldSendingTime = reloader.FixParser_.GetField(f9fix_kTAG_SendingTime);
            if (fon9_LIKELY(fldMsgType && fldSendingTime)) {
               if (const FixMsgTypeConfig* cfg = this->FixConfig_->Get(fldMsgType->Value_)) {
                  // 檢查 MsgType 是否需要 Replay?
                  bool isReplayRequired = cfg->IsInfiniteTTL();
                  if (!isReplayRequired && cfg->TTL_.GetOrigValue() > 0) {
                     TimeStamp tmSent = StrTo(fldSendingTime->Value_, TimeStamp{});
                     isReplayRequired = (UtcNow() < (tmSent + cfg->TTL_));
                  }
                  if (isReplayRequired) {
                     if (this->BeginSeqNo_ < curSeqNo)
                        this->GapFill(this->BeginSeqNo_, curSeqNo, logbuf);
                     if (this->Rebuild(fldMsgType, fldSendingTime, logbuf))
                        this->BeginSeqNo_ = curSeqNo + 1;
                  }
               }
            }
            if (this->EndSeqNo_ != 0 && this->EndSeqNo_ == curSeqNo)
               break;
         }
         this->CurMsg_ = reloader.FindNext(this->FixRecorder_);
      }
   }
   FixRecorder::Locker Run(FwdBufferList& logbuf) {
      FixRecorder::ReloadSent reloader;
      this->CurMsg_ = reloader.Start(this->FixRecorder_, this->BeginSeqNo_);
      for (;;) {
         this->Reload(reloader, logbuf);
         FixRecorder::Locker locker{this->FixRecorder_.Lock()};
         if (this->EndSeqNo_ == 0) { // 檢查是否還有沒載入的資料.
            if (!reloader.IsErrOrEOF(this->FixRecorder_, std::move(locker)))
               continue;
            assert(locker.owns_lock());
            FixSeqNum nextSendSeq = this->FixRecorder_.GetNextSendSeq(locker);
            if (this->BeginSeqNo_ != nextSendSeq) {
               // 已經檢查過 IsErrOrEOF() => 確定已經讀到檔尾.
               // 如果有此情況(this->BeginSeqNo_ != nextSendSeq):
               // 應該是另一 thread 重設了新的序號.
               // 所以要送 SeqReset 給對方!
               bool isGapFill = (this->BeginSeqNo_ < nextSendSeq);
               this->SeqReset(isGapFill, this->BeginSeqNo_, nextSendSeq, logbuf);
               if (isGapFill)
                  FwdPrint(logbuf, f9fix_kCSTR_HdrInfo "Replay.End.GapFill:|NewSeqNo=");
               else
                  FwdPrint(logbuf, f9fix_kCSTR_HdrInfo "Replay.End.SequenceReset:|NewSeqNo=");
               FwdPrint(logbuf, nextSendSeq, '\n');
            }
         }
         else if (this->BeginSeqNo_ < this->EndSeqNo_) {
            this->GapFill(this->BeginSeqNo_, this->EndSeqNo_ + 1, logbuf);
         }
         return locker;
      }
   }
};
fon9_WARN_POP;
//--------------------------------------------------------------------------//
void FixSender::Replay(const FixConfig& fixConfig, FixSeqNum beginSeqNo, FixSeqNum endSeqNo) {
   if (fixConfig.IsNoReplay_) {
      this->GapFill(beginSeqNo, endSeqNo);
      return;
   }
   if (endSeqNo == 0) {
      // 暫停 this->OnSendFixMessage(), 但仍需要寫入 Recorder.
      Locker locker{this->Lock()};
      if (this->IsReplayingAll_)
         return;
      this->IsReplayingAll_ = true;
   }

   FwdBufferList  logbuf{1024};
   FwdPrint(logbuf, f9fix_kCSTR_HdrReplay
            f9fix_kCSTR_SPL "beginSeqNo=", beginSeqNo,
            f9fix_kCSTR_SPL "endSeqNo=", endSeqNo,
            '\n');

   Replayer  replayer{this->GetFixRecorder(), &fixConfig, beginSeqNo, endSeqNo};
   Locker    locker{replayer.Run(logbuf)};
   if (endSeqNo == 0)
      this->IsReplayingAll_ = false;
   this->OnSendFixMessage(locker, replayer.SendBuf_.MoveOut());
   this->Append(std::move(locker), logbuf.MoveOut());
}
void FixSender::GapFill(FixSeqNum beginSeqNo, FixSeqNum endSeqNo) {
   FwdBufferList  logbuf{1024};
   FwdPrint(logbuf, f9fix_kCSTR_HdrReplay
            f9fix_kCSTR_SPL "GapFill"
            f9fix_kCSTR_SPL "beginSeqNo=", beginSeqNo,
            f9fix_kCSTR_SPL "endSeqNo=", endSeqNo,
            '\n');
   Replayer replayer{this->GetFixRecorder(), nullptr, beginSeqNo, endSeqNo};
   Locker   locker{this->Lock()};
   if (endSeqNo == 0) {
      FixSeqNum nextSendSeq = this->GetNextSendSeq(locker);
      bool      isGapFill = (beginSeqNo < nextSendSeq);
      replayer.SeqReset(isGapFill, beginSeqNo, nextSendSeq, logbuf);
      if (isGapFill)
         FwdPrint(logbuf, f9fix_kCSTR_HdrInfo "GapFill:|NewSeqNo=");
      else
         FwdPrint(logbuf, f9fix_kCSTR_HdrInfo "SequenceReset:|NewSeqNo=");
      FwdPrint(logbuf, nextSendSeq, '\n');
   }
   else if (beginSeqNo < endSeqNo)
      replayer.GapFill(beginSeqNo, endSeqNo + 1, logbuf);
   else {
      FwdPrint(logbuf, f9fix_kCSTR_HdrError "beginSeqNo >= endSeqNo\n");
      this->Append(std::move(locker), logbuf.MoveOut());
      return;
   }
   this->OnSendFixMessage(locker, replayer.SendBuf_.MoveOut());
   this->Append(std::move(locker), logbuf.MoveOut());
}

} } // namespaces
