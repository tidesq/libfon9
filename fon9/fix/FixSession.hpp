/// \file fon9/fix/FixSession.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_fix_FixSession_hpp__
#define __fon9_fix_FixSession_hpp__
#include "fon9/fix/FixFeeder.hpp"
#include "fon9/fix/FixReceiver.hpp"

namespace fon9 { namespace fix {

/// \ingroup fix
/// 狀態變化請參考
/// https://docs.google.com/document/d/18j2BElaxeboMyo-JVwhszPEAYfAWCTmeihbKQcEHieg/edit#bookmark=id.jjyii5qlu2si
enum class FixSessionSt {
   /// 尚未連線.
   Disconnected,
   /// 線路已連線.
   Connected,

   /// Logon訊息已送出.
   /// - A方尚未補齊B方的要求之前, B方可能會拋棄A的訊息.
   /// - 所以登入作業必須確定雙方的回補皆已結束, 才能進入 ApReady 狀態.
   /// - 底下的登入狀態變化, 就是為了確保雙方皆已補齊.
   LogonSent,
   /// 如果發現 Logon 訊息有 gap 需要回補,
   /// 則送出 ResendRequest, 然後進入此狀態.
   LogonRecovering,
   /// Logon Recover 已結束, 送出 TestRequest.
   /// - 如果對方回應Hb, 則進入 ApReady.
   /// - 如果收到對方的 ResendRequest, 則補完後再送一次 TestRequest.
   /// - 如果對方無回應, 則斷線.
   LogonTest,

   /// 已完成登入及同步作業, 可以開始送出 Ap 訊息!
   ApReady,

   /// 已送出 Logout 訊息, 等候對方的 Logout 訊息.
   LogoutPending,
};

fon9_WARN_DISABLE_PADDING;
/// \ingroup fix
/// FIX Session.
/// - **Not** thread safe!
///   - 也就是使用者(及衍生者), 必須使用同步機制處理各項操作.
///   - 可參考 IoFixSession: 使用 fon9::io 機制的 FixSession.
/// - 負責協調一組 FixReceiver, FixSender, FixConfig 共同合作
/// - 負責掌握狀態變化.
class fon9_API FixSession : protected FixFeeder, public FixReceiver {
   fon9_NON_COPY_NON_MOVE(FixSession);
   using baseFixReceiver = FixReceiver;
   FixSessionSt   FixSt_{FixSessionSt::Disconnected};
   uint32_t       HeartBtInt_;
   uint32_t       HbTestCount_;
   uint32_t       MsgReceivedCount_;
   FixSeqNum      ResetNextSendSeq_{0}; // 用在 SendLogon() 時, 重設下一個傳送序號.
   FixSeqNum      ResetNextRecvSeq_{0}; // 當設定 FixSender_ 時, 重設下一個接收序號.
   FixRecvEvArgs  RxArgs_;

   /// FixSender 設定時機:
   /// - Initiator: 在 SendLogon() 時.
   /// - Acceptor:  在收到 Logon Message 確定登入成功, 然後 SendLogonResponse() 時.
   /// - 在 OnFixSessionDisconnected() 時歸還給 FixManager.
   FixSenderSP    FixSender_;

   bool Check1stMustLogon();
   void SendLogonTestRequest();
   void SetApReadyStImpl();
   void SetApReadySt() {
      if (this->FixSt_ < FixSessionSt::ApReady)
         this->SetApReadyStImpl();
   }
   void OnLogonResponsed(const FixRecvEvArgs& rxargs, FixReceiver::GapFlags gapf);
   void ClearFixSession(FixSessionSt st);

protected:
   // override FixFeeder.
   void OnFixMessageParsed(StrView fixmsg) override;
   FixParser::Result OnFixMessageError(FixParser::Result res, StrView& fixmsgStream, const char* perr) override;

   // override FixReceiver.
   void OnRecoverDone(const FixRecvEvArgs& rxargs) override;
   void OnLogoutRequired(const FixRecvEvArgs& rxargs, FixBuilder&& fixb) override;

   /// 在指定時間後呼叫 FixSessionOnTimer(); 請參考 IoFixSession;
   virtual void FixSessionTimerRunAfter(TimeInterval after) = 0;
   virtual void FixSessionTimerStopNoWait() = 0;
   void FixSessionOnTimer();

   /// this->Dev_->AsyncLingerClose(std::move(cause));
   virtual void CloseFixSession(std::string cause) = 0;

   /// 必須由衍生者主動呼叫, 參考: `IoFixSession::OnDevice_StateChanged()`
   virtual FixSenderSP OnFixSessionDisconnected(const StrView& info);
   virtual void OnFixSessionConnected();
   /// 檢驗登入訊息: this->FixManager_.OnRecvLogonRequest(rxargs);
   virtual void OnRecvLogonRequest(FixRecvEvArgs& rxargs) = 0;
   /// 強迫送出指定訊息: this->Dev_->Send(std::move(msg));
   /// 目前只有在: 尚未設定 this->FixSender_ 的 SendLogout() 才會呼叫此處。
   virtual void OnFixSessionForceSend(BufferList&& msg) = 0;
   /// 進入 ApReady 狀態: this->FixManager_.OnFixSessionApReady(this);
   virtual void OnFixSessionApReady() = 0;

   /// 重設下一個輸出序號.
   /// - strNum 不正確(有非數字字元), 傳回: "Invalid next SEND seqNum string."
   /// - 若現在 ApReady 則使用 SequenceReset.
   ///   - 傳回: "New next SEND seqNum = nextSendSeq\n" "SequenceReset sent\n"
   /// - 否則等候下次的 SendLogon() 使用新序號.
   ///   - 傳回: "New next SEND seqNum = nextSendSeq\n" "Use it when next Logon.\n"
   std::string ResetNextSendSeq(StrView strNum);
   std::string ResetNextRecvSeq(StrView strNum);

public:
   FixSession(const FixConfig& cfg);
   virtual ~FixSession();

   FixSessionSt GetFixSessionSt() const {
      return this->FixSt_;
   }

   /// - Initiator: 在 OnDevice_LinkReady() 事件:
   ///   - 觸發 this->OnFixSessionConnected();
   ///   - 然後透過 FixManager_.OnFixSessionConnected(*this):
   ///      - FixManager: 取出相關設定.
   ///      - FixManager: 接著送出 Logon 登入系統.
   /// - Acceptor: 收到 Logon 訊息:
   ///   - 觸發 FixManager_.OnRecvLogon();
   ///   - FixManager 根據 FixSession::Config_ 及 收到的 Logon 訊息進行認證.
   ///   - 認證成功, 則透過 FixSession::SendLogon() 回覆登入成功.
   ///   - 認證失敗, 則透過 FixSession::SendLogout() 回覆登入失敗.
   /// - 這裡會在送出的FIX訊息前面加上  "|108=heartBtInt".
   void SendLogon(FixSenderSP fixout, uint32_t heartBtInt, FixBuilder&& fixb);

   /// - SendLogon()
   //  - if (rxargs.SeqSt_ == FixSeqSt::Conform) SendLogonTestRequest;
   /// - if (rxargs.SeqSt_ == FixSeqSt::TooHigh) SendResendRequest;
   void SendLogonResponse(FixSenderSP fixout, uint32_t heartBtInt, FixBuilder&& fixb, const FixRecvEvArgs& rxargs);

   /// - fixb 必須先填妥登出原因.
   /// - fixout 通常用在 Acceptor 尚未 SendLogon() 之前就失敗, 提供 fixout 做為記錄&傳送訊息使用.
   ///          若此時 this->FixSender_ 有效, 則不理會 fixout.
   void SendLogout(FixBuilder&& fixb, FixRecorder* fixr = nullptr);
   void SendLogout(const StrView& text, FixRecorder* fixr = nullptr) {
      FixBuilder  fixb;
      RevPrint(fixb.GetBuffer(), f9fix_SPLTAGEQ(Text), text);
      this->SendLogout(std::move(fixb), fixr);
   }

   void SendSessionReject(FixSeqNum refSeqNum, FixTag refTagID, const StrView& refMsgType, FixBuilder&& fixb);

   /// 設定:
   /// - FixReceiver::InitFixConfig();
   /// - MsgType = Heartbeat   的 FixMsgHandler = &FixSession::OnRecvHeartbeat;
   /// - MsgType = TestRequest 的 FixMsgHandler = &FixSession::OnRecvTestRequest;
   /// - MsgType = Logon       的 FixMsgHandler = &FixSession::OnRecvLogonResponse;
   /// - MsgType = Logout      的 FixMsgHandler = &FixSession::OnRecvLogout;
   static void InitFixConfig(FixConfig& fixcfg);
   static void OnRecvHeartbeat(const FixRecvEvArgs&);
   static void OnRecvTestRequest(const FixRecvEvArgs&);
   static void OnRecvLogonResponse(const FixRecvEvArgs& rxargs);
   static void OnRecvLogout(const FixRecvEvArgs& rxargs);
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_fix_FixSession_hpp__
