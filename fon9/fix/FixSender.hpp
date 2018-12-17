/// \file fon9/fix/FixSender.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_fix_FixSender_hpp__
#define __fon9_fix_FixSender_hpp__
#include "fon9/fix/FixRecorder.hpp"
#include "fon9/fix/FixBuilder.hpp"

namespace fon9 { namespace fix {

fon9_WARN_DISABLE_PADDING;
/// \ingroup fix
/// FIX 訊息傳送控制器.
/// - 由 User 填妥 AP 層訊息後, 由 FixSender 完成完整 FIX Message:
///   - 填妥 BeginHeader, BodyLength, CompIDs, MsgType, CheckSum.
/// - 提供 Replay() 重送功能, 重送期間可暫停傳送即時訊息, 直到 replay 結束.
class fon9_API FixSender : protected FixRecorder {
   fon9_NON_COPY_NON_MOVE(FixSender);
   using base = FixRecorder;
   inline friend void intrusive_ptr_add_ref(const FixSender* p) {
      intrusive_ptr_add_ref(static_cast<const FixRecorder*>(p));
   }
   inline friend void intrusive_ptr_release(const FixSender* p) {
      intrusive_ptr_release(static_cast<const FixRecorder*>(p));
   }

   bool       IsReplayingAll_{false};
   TimeStamp  LastSentTime_;
   struct Replayer;
   void Send(Locker&        locker,
             StrView        fldMsgType,
             FixBuilder&&   fixmsgBuilder,
             FixSeqNum      nextSeqNum,
             RevBufferList* fixmsgDupOut);

protected:
   /// - 在觸發 OnSendFixMessage() 時, 為了確保依序輸出訊息, 所以 fixRecorder 仍在鎖定狀態!
   /// - 如果在 OnSendFixMessage() 之前就解鎖, 可能造成另一個 thread 的 Send() 插入!
   /// - 所以在 OnSendFixMessage() 之前不能解鎖!
   /// - 但事件接收者, 可自行解鎖: 例如: 已將 buf 放入傳送緩衝區, 但還有其他事情要做, 則可以先解鎖.
   /// - buf 可能包含 1..N 筆的訊息.
   virtual void OnSendFixMessage(Locker&, BufferList buf) = 0;

public:
   using base::base;
   virtual ~FixSender();

   using base::BeginHeader_;
   using base::CompIDs_;

   FixRecorder& GetFixRecorder() {
      return *static_cast<FixRecorder*>(this);
   }
   const std::string& GetStorageName() const {
      return this->GetStorage().GetOpenName();
   }
   TimeStamp GetLastSentTime() const {
      return this->LastSentTime_;
   }

   using base::Locker;
   using base::Lock;

   /// 完成 fixmsg 並觸發 OnSendFixMessage 事件.
   /// \param fldMsgType    f9fix_SPLFLDMSGTYPE(NewOrderSingle)
   /// \param fixmsgBuilder 已填妥必要的 AP 欄位之後的訊息, 除了底下欄位:
   ///   - BeginString, BodyLength, CompIDs
   ///   - MsgType
   ///   - MsgSeqNum
   ///   - SendingTime
   /// \param fixmsgDupOut 如果 != nullptr, 則複製一份送出的 FIX Message.
   void Send(const StrView& fldMsgType,
             FixBuilder&&   fixmsgBuilder,
             RevBufferList* fixmsgDupOut = nullptr) {
      Locker locker{this->Lock()};
      this->Send(locker, fldMsgType, std::move(fixmsgBuilder), 0, fixmsgDupOut);
   }
   void Send(Locker&        locker,
             const StrView& fldMsgType,
             FixBuilder&&   fixmsgBuilder,
             RevBufferList* fixmsgDupOut = nullptr) {
      this->Send(locker, fldMsgType, std::move(fixmsgBuilder), 0, fixmsgDupOut);
   }

   /// 重設下一個輸出序號.
   /// - 送出 SequenceReset Message.
   /// - 在 Recorder 寫一個 "RST|S=newSeqNo" 記錄.
   /// - 設定 Recorder NextSendSeq = newSeqNo;
   /// - 所以一旦重設了序號, 對方就無法回補到之前的訊息了!
   void SequenceReset(FixSeqNum newSeqNo);
   /// 強制重設下一個輸出序號.
   /// - **不會** 送出 SequenceReset Message.
   /// - 在 Recorder 寫一個 "RST|S=nextSeqNum" 記錄.
   /// - 設定 Recorder NextSendSeq = nextSeqNum;
   void ResetNextSendSeq(FixSeqNum nextSeqNum);

   /// 重送, 包含 beginSeqNo 及 endSeqNo.
   /// 如果 endSeqNo==0, 則表示暫停正常的傳送, 直到全部重送完畢.
   /// 同一時間僅能有一個 Replay() 呼叫.
   /// \param fixConfig 若 fixConfig.IsNoReplay_ 則直接轉呼叫 this->GapFill();
   ///                  否則載入曾經送出的訊息時, 判斷 FixMsgTypeConfig::TTL_ 的有效時間決定是否要重送.
   void Replay(const FixConfig& fixConfig, FixSeqNum beginSeqNo, FixSeqNum endSeqNo);
   /// 當收到 ResendRequest 時, 不提供 Replay.
   void GapFill(FixSeqNum beginSeqNo, FixSeqNum endSeqNo);
};
fon9_WARN_POP;
using FixSenderSP = fon9::intrusive_ptr<FixSender>;

} } // namespaces
#endif//__fon9_fix_FixSender_hpp__
