// \file fon9/web/WebSocketAuther.cpp
// \author fonwinz@gmail.com
#include "fon9/web/WebSocketAuther.hpp"

namespace fon9 { namespace web {

HttpWebSocketAuthHandler::~HttpWebSocketAuthHandler() {
}
io::RecvBufferSize HttpWebSocketAuthHandler::OnHttpRequest(fon9::io::Device& dev, HttpRequest& req) {
   if (req.MessageSt_ != HttpMessageSt::FullMessage)
      return fon9::io::RecvBufferSize::Default;
   if (IsUpgradeToWebSocket(req.Message_)) {
      fon9::RevBufferList rbuf = UpgradeToWebSocket(dev, req);
      if (rbuf.cfront()) {
         SendWebSocketAccepted(dev, std::move(rbuf));
         static_cast<HttpSession*>(dev.Session_.get())->UpgradeTo(WebSocketSP{new WebSocketAuther(&dev, this)});
         return io::RecvBufferSize::NoLink;
      }
   }
   return OnBadWebSocketRequest(dev, req);
}

WebSocketAuther::WebSocketAuther(io::DeviceSP dev, HttpWebSocketAuthHandlerSP owner)
   : base{std::move(dev)}
   , Owner_{std::move(owner)} {
   std::string mechList = "SASL: " + this->Owner_->AuthMgr_->GetSaslMechList();
   this->Send(WebSocketOpCode::TextFrame, &mechList);
}

io::RecvBufferSize WebSocketAuther::OnWebSocketMessage() {
   if (this->AuthSession_) {
      this->AuthRequest_.Response_ = std::move(this->Payload_);
      AuthSession_->AuthVerify(this->AuthRequest_);
      return io::RecvBufferSize::Default;
   }
   this->AuthRequest_.UserFrom_ = this->Device_->WaitGetDeviceId();

   io::DeviceSP dev = this->Device_;
   this->AuthSession_ = this->Owner_->AuthMgr_->CreateAuthSession(&this->Payload_, // mechName
      [dev](auth::AuthR authr, auth::AuthSessionSP authSession) {
         // 這裡的 callback 必須把 dev 帶進來,
         // 否則如果 dev 死亡 => dev.Session_ 死亡 => 則 this 也會跟著死.
         // 如果還沒認證成功, 則會使用到已死的 this.
      if (auto pthis = dynamic_cast<WebSocketAuther*>(static_cast<HttpSession*>(dev->Session_.get())->GetRecvHandler()))
         pthis->OnAuthVerify(std::move(authr), std::move(authSession));
   });

   if (this->AuthSession_)
      return io::RecvBufferSize::Default;
   std::string cause{"Unknown SASL mech."};
   this->Send(WebSocketOpCode::TextFrame, &cause);
   this->Device_->AsyncClose(std::move(cause));
   return io::RecvBufferSize::CloseRecv;
}
void WebSocketAuther::OnAuthVerify(auth::AuthR authr, auth::AuthSessionSP authSession) {
   assert(authSession == this->AuthSession_);
   if (authr.RCode_ == fon9_Auth_Success) {
      // 如果在此 this->Send(&authr.Info_); 對方已收到[認證成功],
      // 且送來了認證成功後的首筆訊息, 但尚未升級到新的 WebSocket,
      // 則會遺漏訊息!!
      // => 所以需要把最後成功訊息放在 async task 裡面送出.
      this->Device_->OpQueue_.AddTask(io::DeviceAsyncOp{[authr, authSession](io::Device& dev) {
         if (dev.OpImpl_GetState() != io::State::LinkReady)
            return;
         HttpSession* httpSession = static_cast<HttpSession*>(dev.Session_.get());
         if (auto pthis = dynamic_cast<WebSocketAuther*>(httpSession->GetRecvHandler())) {
            pthis->Send(WebSocketOpCode::TextFrame, &authr.Info_);
            if (WebSocketSP ws = pthis->Owner_->CreateWebSocketService(dev, authSession->GetAuthResult()))
               httpSession->UpgradeTo(std::move(ws));
            else
               dev.AsyncClose("WebSocketAuther.CreateWebSocketService()|err=" + authSession->GetAuthResult().ExtInfo_);
         }
      }});
   }
   else {
      this->Send(WebSocketOpCode::TextFrame, &authr.Info_);
      if (authr.RCode_ == fon9_Auth_PassChanged) {
         // TODO: 改密碼成功, 要做些什麼事情呢?
      }
      else if (authr.IsError()) {
         this->Device_->AsyncClose(std::move(authr.Info_));
      }
      else // 認證尚未完成.
         return;
   }
   // 認證完成, 必須清理認證要求物件(AuthSession_), 因為該物件的 callback 包含了 dev, 如果不清理可能會有 memory leak.
   this->AuthSession_.reset();
}

} } // namespaces
