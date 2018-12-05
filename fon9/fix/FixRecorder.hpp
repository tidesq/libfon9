/// \file fon9/fix/FixRecorder.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_fix_FixRecorder_hpp__
#define __fon9_fix_FixRecorder_hpp__
#include "fon9/fix/FixParser.hpp"
#include "fon9/fix/FixCompID.hpp"
#include "fon9/buffer/RevBufferList.hpp"
#include "fon9/FileAppender.hpp"

namespace fon9 { namespace fix {

fon9_WARN_DISABLE_PADDING;
/// \ingroup fix
/// FIX Message 記錄器.
/// - 每個 Session 一個記錄器.
///   - 包含此 Session 的 CompIDs
///   - 包含 BeginHeader: "8=FIX.n.m|9="
///   - 包含預期的下一個序號: GetNextSendSeq(), GetNextRecvSeq()
///     - 並配合「收(FixInput), 送(FixOutput)」管理序號.
/// - 所有收送訊息都依序記錄在此.
/// - 可取出之前送出的訊息(一般用於回報回補).
/// - 存檔格式:
///   \code
///    +---------------------+-- 中間用空白(space) 分隔.
///    |                     |
///   R yyyymmddhhmmss.uuuuuu FIX Message\n
///   | |___________________/
///   | |
///   | +- 寫入時間: 把寫入資料「R time FIX Message\n」打包, 一次寫入.
///   |    收到的訊息: 解析成功後, 若沒有 gap 就會立即寫入, 然後才會進行AP層的處理.
///   |    送出的訊息: 呼叫 SendASAP() 送出前, 會先寫入.
///   |
///   +--- R = 收到的訊息.
///        S = 送出的訊息.
///        "\x02" "IDX:S=NextSendSeq|R=NextRecvSeq\n" = 索引訊息.
///        "\x02" "RST:S=NextSendSeq\n"
///        "\x02" "RST:R=NextSendSeq\n"
///               FIX 有 Sequence Reset 機制, 當發生此情況時, 必定會跟隨一個 RST 訊息.
///        其他 = 額外資訊, 參考 f9fix_kCSTR_Hdr*
///   \endcode
class fon9_API FixRecorder : protected AsyncFileAppender {
   fon9_NON_COPY_NON_MOVE(FixRecorder);
   using base = AsyncFileAppender;
   FixSeqNum   NextSendSeq_{0};
   FixSeqNum   NextRecvSeq_{0};
   size_t      IdxInfoSizeInterval_;
   
   struct FixRevSercher;
   struct LastSeqSearcher;
   struct SentMessageSearcher;
   friend intrusive_ptr<FixRecorder>;
public:
   using Locker = base::WorkContentLocker;

   /// "8=FIX.n.m|9="
   const CharVector  BeginHeader_;
   /// 僅供 FixInput, FixOutput 使用.
   const CompIDs     CompIDs_;

   enum {
      /// timestamp 使用預設格式輸出, 所需要的長度.
      kTimeStampWidth = kDateTimeStrWidth,
      /// 預期的最大 FIX Message 大小.
      kMaxFixMsgBufferSize = 1024 * 4,
      /// 最小的 FIX Message 大小, 通常用在收資料時檢查 bodyLength 使用.
      kMinFixMsgBufferSize = sizeof("8=FIX.n.m|9=nnnn|10=nnn|"),
      /// 取回已送出的訊息時, 使用的緩衝區大小.
      kReloadSentBufferSize = 1024 * 64,
      /// 大約間隔 n KB 寫一個 IDX 訊息.
      kIdxInfoSizeInterval = kReloadSentBufferSize - 1024 * 2,
   };

   FixRecorder(const StrView& beginHeader, CompIDs&& compIDs)
      : BeginHeader_{beginHeader}
      , CompIDs_(std::move(compIDs)) {
   }
   virtual ~FixRecorder();

   using base::WaitFlushed;

   /// 初次建立 FixRecorder:
   /// 1. 開啟記錄檔.
   /// 2. 取得 NextRecvSeq_, NextSeqSeq_
   File::Result Initialize(std::string fileName);

   FixSeqNum GetNextSendSeq(const Locker&) const {
      return this->NextSendSeq_;
   }
   FixSeqNum GetNextRecvSeq() const {
      return this->NextRecvSeq_;
   }

   Locker Lock() {
      return this->Worker_.Lock();
   }

   /// 將 rbuf 寫入, 若已累積超過 kIdxInfoSizeInterval,
   /// 則寫入一條索引訊息: "\x02" "IDX:S=NextSendSeq|R=NextRecvSeq\n".
   void WriteBuffer(Locker& lk, RevBufferList&& rbuf);

   #define f9fix_kCSTR_HdrInfo        "i "
   #define f9fix_kCSTR_HdrError       "e "
   #define f9fix_kCSTR_HdrSend        "S "
   #define f9fix_kCSTR_HdrReplay      "y "
   #define f9fix_kCSTR_HdrRecv        "R "
   #define f9fix_kCSTR_HdrGapRecv     "g "  // 發現 gap, 但此筆訊息保留.
   #define f9fix_kCSTR_HdrIgnoreRecv  "d "
   struct LineHeader {
      char  HeaderStr_[2];
      constexpr LineHeader(const char (&str)[3]) : HeaderStr_{str[0],str[1]} {
         // assert(str[2] == 0);
      }
   };

   template <class... ArgsT>
   void Write(LineHeader header, ArgsT&&... args) {
      RevBufferList rbuf{256 + sizeof(NumOutBuf)};
      RevPrint(rbuf, header.HeaderStr_, UtcNow(), ' ', std::forward<ArgsT>(args)..., '\n');
      auto  lk{this->Worker_.Lock()};
      this->WriteBuffer(lk, std::move(rbuf));
   }

   /// 寫入即將要送出的訊息, 並設定 this->NextSendSeq_;
   /// lineMessage 必須為一行完整的訊息: "S " + timestamp + ' ' + FIX Message + '\n';
   /// 請參考 FixOutput::Send()
   void WriteBeforeSend(Locker& lk, RevBufferList&& lineMessage, FixSeqNum nextSendSeq) {
      this->WriteBuffer(lk, std::move(lineMessage));
      this->NextSendSeq_ = nextSendSeq;
   }

   /// 寫入依正常順序收到的 FIX Message.
   /// 返回前 ++this->NextRecvSeq_;
   void WriteInputConform(const StrView& fixmsg) {
      RevBufferList rbuf{static_cast<BufferNodeSize>(fixmsg.size() + 64)};
      RevPrint(rbuf, f9fix_kCSTR_HdrRecv, UtcNow(), ' ', fixmsg, '\n');
      auto  lk{this->Worker_.Lock()};
      this->WriteBuffer(lk, std::move(rbuf));
      ++this->NextRecvSeq_;
   }
   void WriteInputSeqReset(const StrView& fixmsg, FixSeqNum newSeqNo, bool isGapFill);

   /// 寫入 buf, 前後都不加料.
   void Append(Locker& lk, BufferList&& buf) {
      this->IdxInfoSizeInterval_ += CalcDataSize(buf.cfront());
      WorkContentController* app = static_cast<WorkContentController*>(&WorkContentController::StaticCast(*lk));
      app->AddWork(lk, std::move(buf));
   }

   using PosType = File::PosType;
   /// 取回已送出的訊息.
   class fon9_API ReloadSent {
      fon9_NON_COPY_NON_MOVE(ReloadSent);
      PosType     CurBufferPos_;
      StrView     CurMsg_;
      char        Buffer_[kReloadSentBufferSize + 128];
      const char* DataEnd_;
      friend struct SentMessageSearcher;
      bool InitStart(FixRecorder& fixRecorder, FixSeqNum seqFrom);

   public:
      FixParser  FixParser_;

      ReloadSent() = default;

      /// \retval !empty()
      ///         - 找到的第一筆 sent message.
      ///         - 此筆訊息序號必定 >= seqFrom.
      ///         - 可以從 this->FixParser_.GetMsgSeqNum() 取得找到的訊息序號.
      /// \retval empty()
      ///         - 找不到指定序號的 sent 訊息
      ///         - or 所有訊息都比 seqFrom 小
      ///         - or 有遇到 "RST|S=n" 且在 RST 之後的訊息序號全都 < seqFrom.
      StrView Start(FixRecorder& fixRecorder, FixSeqNum seqFrom);

      /// \retval empty()  找不到指定的訊息.
      /// \retval !empty() 找到指定的訊息, 此筆訊息序號必定 == seq.
      StrView Find(FixRecorder& fixRecorder, FixSeqNum seq);

      /// 下一筆訊息不一定會連號.
      /// 此訊息返回前 **不會** 經由 this->FixParser_ 解析.
      StrView FindNext(FixRecorder& fixRecorder);

      bool IsErrOrEOF(FixRecorder& fixRecorder, FixRecorder::Locker& locker) const;
   };
};
using FixRecorderSP = intrusive_ptr<FixRecorder>;
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_fix_FixRecorder_hpp__
