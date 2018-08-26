// \file fon9/web/HttpSession.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_web_HttpSession_hpp__
#define __fon9_web_HttpSession_hpp__
#include "fon9/web/HttpHandler.hpp"
#include "fon9/framework/IoFactory.hpp"

namespace fon9 { namespace web {

/// \ingroup web
/// 一個簡易的 http server.
/// 接收 http request, 解析並透過 HttpHandler 處理.
class fon9_API HttpSession : public io::Session {
   fon9_NON_COPY_NON_MOVE(HttpSession);
   class Server;

protected:
   web::HttpRequest           Request_;
   const web::HttpHandlerSP   Handler_;

   // 檢查 LinkBroken: 處理最後 chunked 訊息? => http server 不考慮此情況.
   // if (this->Request_.IsChunked())
   //    this->HttpRequestReady();
   //void OnDevice_StateChanged(io::Device& dev, const io::StateChangedArgs& e) override;

   io::RecvBufferSize OnDevice_Recv(io::Device& dev, DcQueueList& rxbuf) override;

   virtual io::RecvBufferSize OnRequestEvent(io::Device& dev);

public:
   HttpSession(web::HttpHandlerSP handler) : Handler_{std::move(handler)} {
   }

   static SessionFactorySP MakeFactory(std::string factoryName, HttpHandlerSP wwwRootHandler);
};

} } // namespaces
#endif//__fon9_web_HttpSession_hpp__
