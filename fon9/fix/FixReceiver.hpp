/// \file fon9/fix/FixReceiver.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_fix_FixReceiver_hpp__
#define __fon9_fix_FixReceiver_hpp__
#include "fon9/fix/FixSender.hpp"
#include "fon9/fix/FixConfig.hpp"
#include <deque>

namespace fon9 { namespace fix {

fon9_WARN_DISABLE_PADDING;
/// \ingroup fix
/// 主要工作:
/// - 負責確保 FIX Message 的連續, 若發現 gap:
///   1. 根據 FixMsgTypeConfig.TTL 決定是否需要保留訊息
///      - IsInfiniteTTL()  需要保留, 例如: ExecutionReport(MsgType=8).
///      - !IsInfiniteTTL() 由對方決定是否要回補, 例如: 下單資料.
///   2. 呼叫 FixSender::Send(ResendRequest) 要求對方回補.
///   3. gap 填補的過程, 可能取回「步驟1.」所保留的訊息.
/// - 收到的訊息寫入 FixRecorder.
/// - 檢查 CompIDs.
/// - 將連續的訊息交給 FixMsgTypeConfig.FixMsgHandler_ 處理.
/// - 必須在 FixSession 處理完 Logon(MsgType=A) 之後使用, 例如:
///   - 由 FixSession 確保:
///      - 連線後的第1個訊息必須是 Logon.
///      - 若在 Logon 成功前, 收到其他的訊息, 應直接斷線!
///   - FixSession 收到 Logon 登入成功訊息.
///   - 呼叫 FixReceiver::Receive():
///   - FixSession 進入 ApReady 狀態.
class fon9_API FixReceiver {
   fon9_NON_COPY_NON_MOVE(FixReceiver);
   struct Msg {
      std::string MsgStr_;
      FixSeqNum   SeqNum_;
      Msg(std::string fixmsg, FixSeqNum seqNum)
         : MsgStr_{std::move(fixmsg)}
         , SeqNum_{seqNum} {
      }
   };
   using MsgList = std::deque<Msg>;
   MsgList     MsgKeeper_;
   FixSeqNum   ResendRequestEndSeqNo_{0};
   FixSeqNum   LastGapSeqNo_{0};

   void CheckMsgKeeper(const FixRecvEvArgs& rxargs, FixSeqNum newSeqNo);
   void SendResendRequest(FixSender& fixSender, FixSeqNum beginSeqNo, FixSeqNum endSeqNo);
   void OnMessageProcessed(const FixRecvEvArgs& rxargs, FixSeqNum msgSeqNumProcessed) {
      if (this->ResendRequestEndSeqNo_ > 0 && msgSeqNumProcessed == this->ResendRequestEndSeqNo_)
         this->CheckMsgKeeper(rxargs, msgSeqNumProcessed + 1);
   }
   /// 如果發現 CompID 不正確, 則呼叫 this->OnBadCompID();
   FixTag CheckCompID(const FixRecvEvArgs& rxargs, const StrView& msgType);

protected:
   void ClearRecvKeeper();
   
   /// 收到的 MsgSeqNum 小於預期, 預設:
   /// - 如果有 PossDup: 寫 log(f9fix_kCSTR_HdrIgnoreRecv), 轉呼叫 OnRecvPossDup();
   /// - 如果沒 PossDup: OnLogoutRequired("58=MsgSeqNum too low...");
   virtual void OnMsgSeqNumTooLow(const FixRecvEvArgs& rxargs);
   /// 收到的訊息小於預期且有設定 PossDup=Y, 預設: do nothing.
   virtual void OnRecvPossDup(const FixRecvEvArgs& rxargs);
   /// 當收到的訊息有問題, 必須送出 Logout(例如 OnMsgSeqNumTooLow()).
   /// 預設: this->FixSender_.Send(f9fix_SPLFLDMSGTYPE(Logout), std::move(fixb));
   virtual void OnLogoutRequired(const FixRecvEvArgs& rxargs, FixBuilder&& fixb);

   /// 找不到 FixMsgHandler, 預設呼叫: SendInvalidMsgType()
   virtual void OnFixMsgHandlerNotFound(const FixRecvEvArgs& rxargs, const StrView& msgType);
   /// 回補完畢. 預設在 Recorder 寫一行 log, 清除 this->ResendRequestEndSeqNo_, this->LastGapSeqNo_;
   /// - 補齊後觸發事件, 讓 Session 進入 ApReady 狀態.
   virtual void OnRecoverDone(const FixRecvEvArgs& rxargs);
   /// 當發現收到的 CompID 不正確.
   /// 預設送出 SessionReject: CompID Problem.
   virtual void OnBadCompID(const FixRecvEvArgs& rxargs, FixTag errTag, const StrView& msgType);

public:
   FixReceiver() = default;
   virtual ~FixReceiver();

   /// rxargs.SeqSt_ 會在呼叫 FixMsgHandler 之前填妥.
   void DispatchFixMessage(FixRecvEvArgs& rxargs);

   enum GapFlags {
      GapDontKeep = 0x00,
      GapKeepRequired = 0x01,
      /// 不用寫任何記錄, 如果沒設定, 預設會寫入底下其中之一:
      /// - `fixRecorder.Write(f9fix_kCSTR_HdrIgnoreRecv, args.MsgStr_);`
      /// - `fixRecorder.Write(f9fix_kCSTR_HdrGapRecv, args.MsgStr_);`
      GapSkipRecord = 0x02,
   };
   void OnMsgSeqNumNotExpected(const FixRecvEvArgs& rxargs, GapFlags flags);

   /// 設定:
   /// - MsgType = SequenceReset 的 FixMsgHandler = &FixReceiver::OnRecvSequenceReset;
   /// - MsgType = ResendRequest 的 FixMsgHandler = &FixReceiver::OnRecvResendRequest;
   static void InitFixConfig(FixConfig& fixcfg);
   /// 由於 SequenceReset-ResetMode 不理會 msgSeqNum 直接設定 newSeqNo.
   /// 所以 FixConfig 必須 FixSeqSt::AllowAnySeq;
   static void OnRecvSequenceReset(const FixRecvEvArgs& rxargs);
   /// 系統預設的 ResendRequest 訊息處理者.
   /// FixConfig 的設定:
   /// \code
   ///   FixMsgTypeConfig& mcfg = fixcfg.Fetch(f9fix_kMSGTYPE_ResendRequest);
   ///   // 不考慮 ResendRequest 的訊息序號是否符合需求, 且呼叫 OnRecvResendRequest() 之前不要寫入 FixRecorder.
   ///   mcfg.FixSeqAllow_ = FixSeqSt::AllowAnySeq | FixSeqSt::NoPreRecord;
   ///   mcfg.FixMsgHandler_ = &OnRecvResendRequest;
   /// \endcode
   static void OnRecvResendRequest(const FixRecvEvArgs& rxargs);
};
fon9_WARN_POP;

inline void FixRecvEvArgs::ResetSeqSt() {
   this->SeqSt_ = CompareFixSeqNum(this->Msg_.GetMsgSeqNum(),
                                   this->FixSender_->GetFixRecorder().GetNextRecvSeq());
}

} } // namespaces
#endif//__fon9_fix_FixReceiver_hpp__
