// \file fon9/framework/HttpManSession.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_framework_HttpManSession_hpp__
#define __fon9_framework_HttpManSession_hpp__
#include "fon9/framework/IoFactory.hpp"
#include "fon9/framework/Framework.hpp"
#include "fon9/web/HttpParser.hpp"

namespace fon9 {

/// 提供一個簡易的 http server 管理 fon9 的 framework.
class HttpManSession : public io::Session {
   fon9_NON_COPY_NON_MOVE(HttpManSession);
   class Server;

   web::HttpMessage Request_;

   // 檢查 LinkBroken: 處理最後 chunked 訊息? => http server 不考慮此情況.
   // if (this->Request_.IsChunked())
   //    this->HttpRequestReady();
   //void OnDevice_StateChanged(io::Device& dev, const io::StateChangedArgs& e) override;

   io::RecvBufferSize OnDevice_Recv(io::Device& dev, DcQueueList& rxbuf) override;

   void HttpRequestReady(io::Device& dev, web::HttpResult res);

public:
   HttpManSession() = default;

   #define fon9_kCSTR_HttpManSession   "HttpMan"
   static SessionFactorySP MakeFactory(Framework& fon9sys);
};

} // namespace
#endif//__fon9_framework_WebManSession_hpp__
