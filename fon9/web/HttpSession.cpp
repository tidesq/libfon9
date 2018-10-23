// \file fon9/web/HttpSession.cpp
// \author fonwinz@gmail.com
#include "fon9/web/HttpSession.hpp"
#include "fon9/web/HttpParser.hpp"
#include "fon9/web/WebSocket.hpp"

namespace fon9 { namespace web {

class HttpSession::Server : public io::SessionServer {
   fon9_NON_COPY_NON_MOVE(Server);
protected:
   const HttpHandlerSP   RootHandler_;
public:
   Server(HttpHandlerSP rootHandler) : RootHandler_{std::move(rootHandler)} {
   }
   io::SessionSP OnDevice_Accepted(io::DeviceServer&) {
      return new HttpSession{this->RootHandler_};
   }
};

SessionFactorySP HttpSession::MakeFactory(std::string factoryName, HttpHandlerSP wwwRootHandler) {
   struct Factory : public SessionFactory {
      fon9_NON_COPY_NON_MOVE(Factory);
      HttpHandlerSP  RootHandler_;
      Factory(std::string&& factoryName, HttpHandlerSP&& wwwRootHandler)
         : SessionFactory(std::move(factoryName))
         , RootHandler_(std::move(wwwRootHandler)) {
      }
      io::SessionSP CreateSession(IoManager&, const IoConfigItem&, std::string& errReason) override {
         // 僅允許透過 server 建立, 所以這裡不提供 session.
         errReason = this->Name_ + ": Only support server device.";
         return io::SessionSP{};
      }
      io::SessionServerSP CreateSessionServer(IoManager&, const IoConfigItem&, std::string& errReason) override {
         (void)errReason;
         return new HttpSession::Server{this->RootHandler_};
      }
   };
   return new Factory{std::move(factoryName), std::move(wwwRootHandler)};
}

HttpSession::HttpSession(HttpHandlerSP rootHandler)
   : RecvHandler_{new HttpMessageReceiver{std::move(rootHandler)}} {
}
void HttpSession::UpgradeTo(HttpRecvHandlerSP ws) {
   this->RecvHandler_ = std::move(ws);
   this->RecvHandler_->OnUpgraded(*this);
   // 如果是 HttpSession client, 則應保留原始 RecvHandler, 斷線後應還原「原始 RecvHandler」!
}
io::RecvBufferSize HttpSession::OnDevice_Recv(io::Device& dev, DcQueueList& rxbuf) {
   return this->RecvHandler_->OnDevice_Recv(dev, rxbuf);
}
void HttpSession::OnDevice_StateChanged(io::Device& dev, const io::StateChangedArgs& e) {
   (void)dev;
   if (e.BeforeState_ == io::State::LinkReady) {
      // 如果是 HttpSession client, 則應保留原始 RecvHandler, 斷線後應還原「原始 RecvHandler」!
      this->RecvHandler_.reset();
   }
}

//--------------------------------------------------------------------------//

HttpRecvHandler::~HttpRecvHandler() {
}
void HttpRecvHandler::OnUpgraded(HttpSession&) {
}

io::RecvBufferSize HttpMessageReceiver::OnRequestEvent(io::Device& dev) {
   if (!this->Request_.IsHeaderReady())
      this->Request_.ParseStartLine();
   return this->RootHandler_->OnHttpRequest(dev, this->Request_);
}

io::RecvBufferSize HttpMessageReceiver::OnDevice_Recv(io::Device& dev, DcQueueList& rxbuf) {
   HttpResult res = HttpParser::Feed(this->Request_.Message_, rxbuf.MoveOut());
   io::RecvBufferSize retval;
   for (;;) {
      switch (res) {
      default:
         dev.AsyncClose("Invalid http result");
         return io::RecvBufferSize::CloseRecv;
      case HttpResult::HeaderTooLarge:
         dev.AsyncClose("Http header too large");
         return io::RecvBufferSize::CloseRecv;
      case HttpResult::BadChunked:
         dev.AsyncClose("Http bad chunked");
         return io::RecvBufferSize::CloseRecv;
      case HttpResult::ChunkSizeTooLarge:
         dev.AsyncClose("Http chunked size too large");
         return io::RecvBufferSize::CloseRecv;
      case HttpResult::ChunkSizeLineTooLong:
         dev.AsyncClose("Http chunked size line too long");
         return io::RecvBufferSize::CloseRecv;

      case HttpResult::Incomplete:
         if (this->Request_.Message_.IsHeaderReady() && !this->Request_.IsHeaderReady())
            return this->OnRequestEvent(dev);
         return io::RecvBufferSize::Default;

      case HttpResult::FullMessage:
         this->Request_.MessageSt_ = HttpMessageSt::FullMessage;
         if ((retval = this->OnRequestEvent(dev)) >= io::RecvBufferSize::Default) {
            this->Request_.RemoveFullMessage();
            break;
         }
__BREAK_RECV:
         if(retval == io::RecvBufferSize::NoLink)
            return io::RecvBufferSize::Default;
         return retval;
      case HttpResult::ChunkAppended:
         this->Request_.MessageSt_ = HttpMessageSt::ChunkAppended;
         retval = this->OnRequestEvent(dev);
         if (retval < io::RecvBufferSize::Default) // 不再接收訊息?!
            goto __BREAK_RECV;
         break;
      }
      res = HttpParser::ContinueEat(this->Request_.Message_);
   }
}

} } // namespaces
