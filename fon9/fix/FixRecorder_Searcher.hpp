// \file fon9/fix/FixRecorder_Searcher.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fix_FixRecorder_Searcher_hpp__
#define __fon9_fix_FixRecorder_Searcher_hpp__
#include "fon9/fix/FixRecorder.hpp"
#include "fon9/FileRevRead.hpp"

namespace fon9 { namespace fix {

#define f9fix_kCHAR_HdrCtrlMsgSeqNum   '\x02'
#define f9fix_kCSTR_HdrCtrlMsgSeqNum   "\x02"
#define f9fix_kCSTR_HdrIdx            f9fix_kCSTR_HdrCtrlMsgSeqNum "IDX"
#define f9fix_kCSTR_HdrRst            f9fix_kCSTR_HdrCtrlMsgSeqNum "RST"
#define f9fix_kCSTR_HdrNextSendSeq    f9fix_kCSTR_SPL "S="
#define f9fix_kCSTR_HdrNextRecvSeq    f9fix_kCSTR_SPL "R="

enum : size_t {
   kControlMsgSeqNumMaxLength = sizeof(f9fix_kCSTR_HdrIdx) - 1 + sizeof("|S=9876543210|R=9876543210\n") - 1,
   kControlMsgSeqNumMinLength = sizeof(f9fix_kCSTR_HdrIdx) - 1 + sizeof("|S=1") - 1,
   kLastSeqSearcherBlockSize = 1024 * 4,
};

fon9_WARN_DISABLE_PADDING;
struct FixRecorder::FixRevSercher : public FileRevSearch {
   fon9_NON_COPY_NON_MOVE(FixRevSercher);
   FixParser&  FixParser_;
   enum RstFlags {
      RstNone = 0x00,
      RstRecv = 0x01,
      RstSend = 0x02,
   };
   RstFlags    RstFlags_{RstNone};

   FixRevSercher(FixParser& fixParser, size_t blockSize, size_t maxMessageSize, void* blockBuffer)
      : FileRevSearch{blockSize, maxMessageSize, blockBuffer}
      , FixParser_(fixParser) {
   }

   /// *pbeg == f9fix_kCHAR_HdrCtrlMsgSeqNum; 解析 pbeg+1:
   ///   - "IDX|S=NextSendSeq|R=NextRecvSeq"
   ///   - "RST|S=NextSendSeq"
   ///   - "RST|S=NextRecvSeq"
   ///
   /// 如果 outNextSend && *outNextSend<=0 才需解析 NextSendSeq,
   /// 如果 outNextRecv && *outNextRecv<=0 才需解析 NextRecvSeq.
   ///
   /// \retval nullptr  格式錯誤.
   /// \retval !nullptr 解析後的尾端位置.
   const char* ParseControlMsgSeqNum(const char* pbeg, const char* pend, FixSeqNum* outNextSend, FixSeqNum* outNextRecv);

   /// 解析 "R timestamp FIX Message" or "S timestamp FIX Message".
   /// 不理會 *pbeg, 但後續格式必須符合 " timestamp ".
   /// \retval nullptr  呼叫時seqn>0 or 格式錯誤.
   /// \retval !nullptr FIX Message 開始位置.
   const char* ParseFixLine_MsgSeqNum(const char* pbeg, const char* pend, FixSeqNum& seqn);
};

struct FixRecorder::LastSeqSearcher : public FileRevReadBuffer<kLastSeqSearcherBlockSize, kMaxFixMsgBufferSize>, public FixRevSercher {
   fon9_NON_COPY_NON_MOVE(LastSeqSearcher);
   LastSeqSearcher(FixParser& fixParser) : FixRevSercher{fixParser, kLastSeqSearcherBlockSize, kMaxFixMsgBufferSize, BlockBuffer_} {
   }

   FixSeqNum   NextSendSeq_ = 0;
   FixSeqNum   NextRecvSeq_ = 0;

   virtual LoopControl OnFileBlock(size_t rdsz) override;

   /// *pbeg=='\n', 解析:
   /// - f9fix_kCSTR_HdrCtrlMsgSeqNum line.
   /// - "R timestamp FIX Message"
   /// - "S timestamp FIX Message"
   LoopControl OnFoundChar(char* pbeg, char* pend) override;

   void ParseFixLine_MsgSeqNum_ToNextSeq(const char* pbeg, const char* pend, FixSeqNum& seqn);
};

struct FixRecorder::SentMessageSearcher : public FileRevRead, public FixRevSercher {
   fon9_NON_COPY_NON_MOVE(SentMessageSearcher);
   SentMessageSearcher(ReloadSent& reloader)
      : FixRevSercher{reloader.FixParser_, kReloadSentBufferSize, sizeof(reloader.Buffer_) - kReloadSentBufferSize, reloader.Buffer_}
   {
      static_assert(sizeof(reloader.Buffer_) >= kReloadSentBufferSize + kControlMsgSeqNumMaxLength, "ReloadSent.Buffer_[] too small.");
   }
   using base = FileRevRead;
   FixSeqNum   ExpectSeq_;
   char        Filler_[4];
   const char* PtrAtBefore_;
   const char* PtrAtAfter_;
   StrView     FoundLine_;
   PosType     PosAtBefore_;
   PosType     PosAtAfter_;
   using FixRevSercher::DataEnd_;

   File::Result Start(ReloadSent& reloader, FixSeqNum expectSeq, File& fd);
   virtual LoopControl OnFileBlock(size_t rdsz) override;
   virtual LoopControl OnFoundChar(char* pbeg, char* pend) override;

   void SetAtAfter(const char* p) {
      this->PtrAtAfter_ = p;
      this->PosAtAfter_ = this->GetBlockPos() + (p - this->BlockBufferPtr_);
   }
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_fix_FixRecorder_Searcher_hpp__
