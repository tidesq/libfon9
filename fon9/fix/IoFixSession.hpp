/// \file fon9/fix/IoFixSession.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_fix_IoFixSession_hpp__
#define __fon9_fix_IoFixSession_hpp__
#include "fon9/fix/FixSession.hpp"
#include "fon9/io/Device.hpp"

namespace fon9 { namespace fix {

fon9_WARN_DISABLE_PADDING;
class fon9_API IoFixSession;

/// \ingroup fin
/// 管理 IoFixSession & FixSender.
class fon9_API IoFixManager {
   fon9_NON_COPY_NON_MOVE(IoFixManager);
protected:
   /// - Write info: from.GetDevuceId();
   /// - fixses.SendLogon();
   void OnLogonInitiate(IoFixSession& fixses, uint32_t heartBtInt, FixBuilder&& fixb, FixSenderSP fixout);

   /// 在身分認證成功之後呼叫.
   /// - 此時的 rxargs.FixSender_ 必定為 nullptr.
   /// - 檢查 msgSeqNum
   /// - 檢查 HeartBtInt
   /// - 檢查 EncryptMethod
   ///
   /// \retval true  成功,返回前已呼叫 FixSession::SendLogonResponse();
   /// \retval false 失敗,欄位有誤,並已將錯誤訊息寫入 FixRecorder,
   ///               返回前已呼叫 FixSession::SendLogout();
   bool OnLogonAccepted(FixRecvEvArgs& rxargs, FixSenderSP fixout);

public:
   IoFixManager() = default;
   virtual ~IoFixManager();

   /// 如果 fixses 的角色是 Initiator 則:
   /// 應在此呼叫 fixses.SendLogon(); 或由 fixses 自行呼叫, 由 fixses 與 IoFixManager 的設計者自行協調.
   virtual void OnFixSessionConnected(IoFixSession& fixses) = 0;
   /// 斷線後歸還 fixSender.
   virtual void OnFixSessionDisconnected(IoFixSession& fixses, FixSenderSP&& fixSender) = 0;

   /// 只有在 fixses 的角色是 Acceptor 時才會呼叫此處, 檢查登入要求.
   /// - 檢查登入訊息: CompIDs, RawData... 驗證使用者.
   /// - 驗證失敗: 直接回覆 rxargs.FixSession_->SendLogout();
   /// - 驗證成功: 取得 FixSender, 送出 FixSession.SendLogonResponse(); 回覆登入結果,
   ///   在 FixSession.SendLogonResponse() 裡面, 除了 SendLogon() 之外, 額外的工作:
   ///   - 檢查 rxargs.Msg_ 是否有 Gap? 若有則送出 ResendRequest.
   ///   - 若沒有 Gap, 則 SendLogonTestRequest.
   /// - 何時 ApReady?
   ///   - 等我方要求的 ResendRequest 補齊.
   ///   - 等對方要求的 ResendRequest 結束.
   ///     - 若對方遲遲沒有 ResendRequest?
   ///     - 則在一段時間後送出 TestRequest 等對方的回覆.
   virtual void OnRecvLogonRequest(FixRecvEvArgs& rxargs) = 0;

   /// 登入協商完成(包含 HbTest 確認雙方序號連續), 可進入「Ap層」作業.
   virtual void OnFixSessionApReady(IoFixSession& fixses) = 0;
};

/// \ingroup fix
/// 使用 fon9::io 機制的 FixSession.
class fon9_API IoFixSession : public io::Session, public FixSession {
   fon9_NON_COPY_NON_MOVE(IoFixSession);
   using baseIo = io::Session;
   using baseFix = FixSession;
   // 在 OnDevice_Initialized() 時設定.
   io::Device*   Dev_{nullptr};

protected:
   // override io::Session
   void OnDevice_Initialized(io::Device& dev) override;
   void OnDevice_Destructing(io::Device& dev) override;
   void OnDevice_StateChanged(io::Device& dev, const io::StateChangedArgs& e) override;
   io::RecvBufferSize OnDevice_LinkReady(io::Device& dev) override;
   io::RecvBufferSize OnDevice_Recv(io::Device& dev, DcQueueList& rxbuf) override;
   std::string SessionCommand(io::Device& dev, StrView cmdln) override;
   void OnDevice_CommonTimer(io::Device& dev, TimeStamp now) override;

   // override FixSession
   void FixSessionTimerRunAfter(TimeInterval after) override;
   void FixSessionTimerStopNoWait() override;
   void CloseFixSession(std::string cause) override;

   void OnRecvLogonRequest(FixRecvEvArgs& rxargs) override;
   void OnFixSessionForceSend(BufferList&& msg) override;
   void OnFixSessionApReady() override;

public:
   IoFixManager&  FixManager_;
   IoFixSession(IoFixManager& mgr, const FixConfig& cfg)
      : baseFix{cfg}
      , FixManager_(mgr) {
   }

   /// 通常只會在 OnDevice_LinkReady() 事件時,
   /// 檢查連線來源的「黑白名單」時才會用到.
   std::string GetDeviceId() const {
      return this->Dev_ ? this->Dev_->WaitGetDeviceId() : std::string{};
   }

   io::Device* GetDevice() const {
      return this->Dev_;
   }
};
fon9_WARN_POP;
using IoFixSessionSP = intrusive_ptr<IoFixSession>;

} } // namespaces
#endif//__fon9_fix_IoFixSession_hpp__
