// \file fon9/framework/HttpManSession.cpp
// \author fonwinz@gmail.com
#include "fon9/framework/HttpManSession.hpp"
#include "fon9/framework/IoManager.hpp"
#include "fon9/RevPrint.hpp"

namespace fon9 {

class HttpManSession::Server : public io::SessionServer {
   fon9_NON_COPY_NON_MOVE(Server);
   Framework&  Fon9Sys_;
public:
   Server(Framework& fon9sys) : Fon9Sys_(fon9sys) {
   }
   io::SessionSP OnDevice_Accepted(io::DeviceServer&) override {
      return new HttpManSession{};
   }
};
SessionFactorySP HttpManSession::MakeFactory(Framework& fon9sys) {
   struct Factory : public SessionFactory {
      fon9_NON_COPY_NON_MOVE(Factory);
      Framework&  Fon9Sys_;
      Factory(Framework& fon9sys)
         : SessionFactory(fon9_kCSTR_HttpManSession)
         , Fon9Sys_(fon9sys) {
      }
      io::SessionSP CreateSession(IoManager&, const IoConfigItem&) override {
         return io::SessionSP{}; // 僅允許透過 server 建立, 所以這裡不提供 session.
      }
      io::SessionServerSP CreateSessionServer(IoManager&, const IoConfigItem&) override {
         return new HttpManSession::Server{this->Fon9Sys_};
      }
   };
   return new Factory{fon9sys};
}
//--------------------------------------------------------------------------//

void HttpManSession::HttpRequestReady(io::Device& dev, web::HttpResult res) {
   if (res == web::HttpResult::ChunkAppended) {
      printf("\n" "ChunkAppended:\n" "chunk-ext=[%s]\n" "chunk-data=[%s]\n",
             this->Request_.ChunkExt().ToString().c_str(),
             this->Request_.ChunkData().ToString().c_str());
      return;
   }
   // web::HttpResult::FullMessage
   printf("\n" "FullMessage:[%s]\n" "[%s]\n",
          this->Request_.StartLine().ToString().c_str(),
          this->Request_.Body().ToString().c_str());
   if (this->Request_.IsChunked())
      printf("chunk-ext=[%s]\n" "trailer=[%s]\n",
             this->Request_.ChunkExt().ToString().c_str(),
             this->Request_.ChunkTrailer().ToString().c_str());

   StrView startline = this->Request_.StartLine();
   StrView method = StrFetchNoTrim(startline, ' ');
   StrView target = StrFetchNoTrim(startline, ' ');
   if (target != "/") {
      dev.StrSend(StrView{"HTTP/1.1 404 OK\r\n\r\n"});
      return;
   }
   dev.StrSend(StrView{
      "HTTP/1.1 200 OK\r\n"
      "Server: Fon9/MaWeb\r\n"
      "Content-Type: text/html\r\n"
            #if 0
               "Content-Length: 52\r\n"
               "\r\n"
               "<html><body>\r\n"
               "<h1>Hello, World!</h1>\r\n"
               "</body></html>"
            #else
               "Transfer-Encoding: chunked\r\n"
               "Connection: close\r\n"
               "\r\n"

               "3f\r\n"
               "<!doctype html><html><body><h1>Hello, World!</h1></body></html>"
               "\r\n"
               "0\r\n\r\n"
            #endif
   });
}
io::RecvBufferSize HttpManSession::OnDevice_Recv(io::Device& dev, DcQueueList& rxbuf) {
   web::HttpResult res = web::HttpParser::Feed(this->Request_, rxbuf.MoveOut());
   for (;;) {
      switch (res) {
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
         return io::RecvBufferSize::Default;
      case web::HttpResult::FullMessage:
      case web::HttpResult::ChunkAppended:
         this->HttpRequestReady(dev, res);
         if (res == web::HttpResult::FullMessage)
            this->Request_.RemoveFullMessage();
         res = web::HttpParser::ContinueEat(this->Request_);
         continue;
      }
      dev.AsyncClose("Invalid http result");
      return io::RecvBufferSize::CloseRecv;
   }
}

} // namespace
