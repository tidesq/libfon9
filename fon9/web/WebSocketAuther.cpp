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
   this->AuthSession_ = this->Owner_->AuthMgr_->CreateAuthSession(
      &this->Payload_, // mechName
      std::bind(&WebSocketAuther::OnAuthVerify, this, std::placeholders::_1, std::placeholders::_2));
   if (this->AuthSession_)
      return io::RecvBufferSize::Default;
   std::string cause{"Unknown SASL mech."};
   this->Send(WebSocketOpCode::TextFrame, &cause);
   this->Device_->AsyncClose(std::move(cause));
   return io::RecvBufferSize::CloseRecv;
}
void WebSocketAuther::OnAuthVerify(auth::AuthR authr, auth::AuthSessionSP authSession) {
   assert(authSession == this->AuthSession_);
   this->Send(WebSocketOpCode::TextFrame, &authr.Info_);
   if (authr.IsError())
      this->Device_->AsyncClose(std::move(authr.Info_));
   else if (authr.RCode_ == fon9_Auth_Success) {
      auto owner = this->Owner_;
      this->Device_->OpQueue_.AddTask(io::DeviceAsyncOp{[owner, authSession](io::Device& dev) {
         if (dev.OpImpl_GetState() != io::State::LinkReady)
            return;
         if (WebSocketSP ws = owner->CreateWebSocketService(dev, authSession->GetAuthResult()))
            static_cast<HttpSession*>(dev.Session_.get())->UpgradeTo(std::move(ws));
         else
            dev.AsyncClose("WebSocketAuther|err=CreateWebSocketService()");
      }});
   }
   else if (authr.RCode_ == fon9_Auth_PassChanged) {
   }
}

} } // namespaces
