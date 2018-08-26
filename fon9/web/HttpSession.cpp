// \file fon9/web/HttpSession.cpp
// \author fonwinz@gmail.com
#include "fon9/web/HttpSession.hpp"
#include "fon9/web/HttpParser.hpp"

namespace fon9 { namespace web {

class HttpSession::Server : public io::SessionServer {
   fon9_NON_COPY_NON_MOVE(Server);
protected:
   const web::HttpHandlerSP   Handler_;
public:
   Server(web::HttpHandlerSP handler) : Handler_{std::move(handler)} {
   }
   io::SessionSP OnDevice_Accepted(io::DeviceServer&) {
      return new HttpSession{this->Handler_};
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
      io::SessionSP CreateSession(IoManager&, const IoConfigItem&) override {
         return io::SessionSP{}; // 僅允許透過 server 建立, 所以這裡不提供 session.
      }
      io::SessionServerSP CreateSessionServer(IoManager&, const IoConfigItem&) override {
         return new web::HttpSession::Server{this->RootHandler_};
      }
   };
   return new Factory{std::move(factoryName), std::move(wwwRootHandler)};
}

//--------------------------------------------------------------------------//

io::RecvBufferSize HttpSession::OnDevice_Recv(io::Device& dev, DcQueueList& rxbuf) {
   web::HttpResult res = web::HttpParser::Feed(this->Request_.Message_, rxbuf.MoveOut());
   io::RecvBufferSize retval;
   for (;;) {
      switch (res) {
      default:
         dev.AsyncClose("Invalid http result");
         return io::RecvBufferSize::CloseRecv;
      case web::HttpResult::HeaderTooLarge:
         dev.AsyncClose("Http header too large");
         return io::RecvBufferSize::CloseRecv;
      case web::HttpResult::BadChunked:
         dev.AsyncClose("Http bad chunked");
         return io::RecvBufferSize::CloseRecv;
      case web::HttpResult::ChunkSizeTooLarge:
         dev.AsyncClose("Http chunked size too large");
         return io::RecvBufferSize::CloseRecv;
      case web::HttpResult::ChunkSizeLineTooLong:
         dev.AsyncClose("Http chunked size line too long");
         return io::RecvBufferSize::CloseRecv;

      case web::HttpResult::Incomplete:
         if (this->Request_.Message_.IsHeaderReady() && !this->Request_.IsHeaderReady())
            return this->OnRequestEvent(dev);
         return io::RecvBufferSize::Default;

      case web::HttpResult::FullMessage:
         this->Request_.MessageSt_ = web::HttpMessageSt::FullMessage;
         retval = this->OnRequestEvent(dev);
         this->Request_.RemoveFullMessage();
         break;
      case web::HttpResult::ChunkAppended:
         this->Request_.MessageSt_ = web::HttpMessageSt::ChunkAppended;
         retval = this->OnRequestEvent(dev);
         break;
      }
      if (retval < io::RecvBufferSize::Default) // 不再接收訊息?!
         return retval;
      res = web::HttpParser::ContinueEat(this->Request_.Message_);
   }
}
io::RecvBufferSize HttpSession::OnRequestEvent(io::Device& dev) {
   if (!this->Request_.IsHeaderReady())
      this->Request_.ParseStartLine();
   return this->Handler_->OnHttpRequest(dev, this->Request_);
}

} } // namespaces
