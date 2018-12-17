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
void FixSender::Send(Locker&        locker,
                     StrView        fldMsgType,
                     FixBuilder&&   fixmsgBuilder,
                     FixSeqNum      nextSeqNum,
                     RevBufferList* fixmsgDupOut) {
   // CompIDs
   RevBuffer& msgRBuf = fixmsgBuilder.GetBuffer();
   RevPrint(msgRBuf, this->CompIDs_.Header_);

   // 底下的欄位順序不可改變, 因為 Replayer::Rebuild() 依賴此順序重建要 replay 的訊息.
   // BeginString|9=BodyLength|SendingTime|MsgType|MsgSeqNum
   //               \____________________/ 這些是 replay 會改變的內容.
   //
   FixSeqNum msgSeqNum = this->GetNextSendSeq(locker);
   RevPrint(msgRBuf, fldMsgType, f9fix_SPLTAGEQ(MsgSeqNum), msgSeqNum);
   // SendingTime.
   TimeStamp now = this->LastSentTime_ = UtcNow();
   RevPut_TimeFIXMS(msgRBuf, fon9::UtcNow());
   RevPrint(msgRBuf, f9fix_SPLTAGEQ(SendingTime));

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
   this->WriteBeforeSend(locker, std::move(rlog), nextSeqNum);
   // 送出訊息.
   if(fon9_LIKELY(!this->IsReplayingAll_))
      this->OnSendFixMessage(locker, std::move(fixmsg));
   else
      DcQueueList{std::move(fixmsg)}.PopConsumed(fixmsgSize);
}

void FixSender::ResetNextSendSeq(FixSeqNum nextSeqNum) {
   if (nextSeqNum <= 0)
      return;
   Locker        locker{this->Lock()};
   RevBufferList rlog{128};
   RevPrint(rlog, f9fix_kCSTR_HdrRst f9fix_kCSTR_HdrNextSendSeq, nextSeqNum, '\n');
   this->WriteBeforeSend(locker, std::move(rlog), nextSeqNum);
}
void FixSender::SequenceReset(FixSeqNum newSeqNo) {
   FixBuilder msgSequenceReset;
   RevPrint(msgSequenceReset.GetBuffer(), f9fix_SPLTAGEQ(NewSeqNo), newSeqNo);
   Locker     locker{this->Lock()};
   this->Send(locker, f9fix_SPLFLDMSGTYPE(SequenceReset), std::move(msgSequenceReset), newSeqNo, nullptr);
}

//--------------------------------------------------------------------------//

// 找到舊訊息的:
// - MsgType: 決定此筆訊息是否需要 Replay
// - MsgSeqNum:
//    - 是否需要 Sequence Reset-GapFill?
//    - 是否已到所要求的最後一筆?
// - SendingTime 的位置:
//   然後將 "SendingTime" 改成: "PossDupFlag=Y|SendingTime=now|OrigSendTime"
//   "|52=" => "|43=Y|52=now|122="
//    \__/      \____ReFlds_____/  直接替換即可, 其餘訊息完全不須變動, 只需重算 BodyLength 及 CheckSum.
//
fon9_WARN_DISABLE_PADDING;
struct FixSender::Replayer {
   fon9_NON_COPY_NON_MOVE(Replayer);

   #define REFLDS_HEAD_CSTR   f9fix_SPLTAGEQ(PossDupFlag) "Y" f9fix_SPLTAGEQ(SendingTime)
   #define REFLDS_CSTR        REFLDS_HEAD_CSTR "yyyymmdd-hh:mm:ss.nnn" f9fix_SPLTAGEQ(OrigSendingTime)
   enum {
      kReFldsWidth = sizeof(REFLDS_CSTR) - 1
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
      memcpy(this->ReFlds_, REFLDS_CSTR, kReFldsWidth);
      ToStrRev_FIXMS(ReFlds_ + sizeof(REFLDS_HEAD_CSTR) - 1 + kDateTimeStrWidth_FIXMS, UtcNow());
   }
   #undef REFLDS_CSTR
   #undef REFLDS_HEAD_CSTR

   bool Rebuild(const FixParser::FixField* fldSendingTime, FwdBufferList& logbuf) {
      const char* const pendCurMsg = this->CurMsg_.end();
      const char*       pvalSendingTime = fldSendingTime->Value_.begin();
      byte* const       pbegNode = reinterpret_cast<byte*>(logbuf.AllocSuffix(this->CurMsg_.size() + kReFldsWidth));
      byte*             pout = pbegNode;
      pout = PutFwd(pout, this->FixRecorder_.BeginHeader_.begin(), this->FixRecorder_.BeginHeader_.size());

      NumOutBuf    numbuf;
      const size_t bodyLength = static_cast<size_t>(pendCurMsg - pvalSendingTime - kFixTailWidth + kReFldsWidth);
      pout = PutFwd(pout, ToStrRev(numbuf.end(), bodyLength), numbuf.end());
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
      if (isGapFill)
         RevPrint(msgSequenceReset.GetBuffer(), f9fix_SPLTAGEQ(GapFillFlag) "Y");
      RevPrint(msgSequenceReset.GetBuffer(), f9fix_SPLTAGEQ(NewSeqNo), newSeqNo);
      // ----- header -----
      RevPrint(msgSequenceReset.GetBuffer(), this->FixRecorder_.CompIDs_.Header_);
      RevPrint(msgSequenceReset.GetBuffer(), f9fix_SPLFLDMSGTYPE(SequenceReset) f9fix_SPLTAGEQ(MsgSeqNum), oldSeqNo);
      RevPutMem(msgSequenceReset.GetBuffer(), this->ReFlds_, kReFldsWidth - (sizeof(f9fix_SPLTAGEQ(OrigSendingTime)) - 1));

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
                     if (this->Rebuild(fldSendingTime, logbuf))
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
            if (!reloader.IsErrOrEOF(this->FixRecorder_, locker))
               continue;
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
   this->Append(locker, logbuf.MoveOut());
   if (endSeqNo == 0)
      this->IsReplayingAll_ = false;
   this->OnSendFixMessage(locker, replayer.SendBuf_.MoveOut());
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
      this->Append(locker, logbuf.MoveOut());
      return;
   }
   this->Append(locker, logbuf.MoveOut());
   this->OnSendFixMessage(locker, replayer.SendBuf_.MoveOut());
}

} } // namespaces
