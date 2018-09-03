// \file fon9/web/WebSocketAuther.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_web_WebSocketAuther_hpp__
#define __fon9_web_WebSocketAuther_hpp__
#include "fon9/web/WebSocket.hpp"
#include "fon9/auth/AuthMgr.hpp"

namespace fon9 { namespace web {

/// \ingroup web
/// 處理升級到 WebSocket 的要求, 並透過 SASL 驗證成功後, 再次升級 WebSocket:
/// - OnHttpRequest(): 處理 upgrade ws 的 http 要求.
/// - 升級後, 先建立 WebSocketAuther 處理 SASL 認證協商.
/// - 認證成功後, 透過 CreateWebSocketService() 建立最終的 WebSocket 服務物件.
class fon9_API HttpWebSocketAuthHandler : public HttpHandler {
   fon9_NON_COPY_NON_MOVE(HttpWebSocketAuthHandler);
   using base = HttpHandler;

protected:
   io::RecvBufferSize OnHttpRequest(io::Device& dev, HttpRequest& req) override;

public:
   const auth::AuthMgrSP AuthMgr_;

   template <class... ArgsT>
   HttpWebSocketAuthHandler(auth::AuthMgrSP authMgr, ArgsT&&... args)
      : base{std::forward<ArgsT>(args)...}
      , AuthMgr_{std::move(authMgr)} {
   }
   ~HttpWebSocketAuthHandler();

   /// 若傳回 nullptr, 則 authResult.ExtInfo_ 必須包含錯誤原因.
   virtual WebSocketSP CreateWebSocketService(io::Device& dev, auth::AuthResult& authResult) = 0;
};
using HttpWebSocketAuthHandlerSP = intrusive_ptr<HttpWebSocketAuthHandler>;

class fon9_API WebSocketAuther : public WebSocket {
   fon9_NON_COPY_NON_MOVE(WebSocketAuther);
   using base = WebSocket;
   auth::AuthSessionSP AuthSession_;
   auth::AuthRequest   AuthRequest_;
   io::RecvBufferSize OnWebSocketMessage() override;
   void OnAuthVerify(auth::AuthR authr, auth::AuthSessionSP authSession);

public:
   const HttpWebSocketAuthHandlerSP   Owner_;

   WebSocketAuther(io::DeviceSP dev, HttpWebSocketAuthHandlerSP owner);
};

} } // namespaces
#endif//__fon9_web_WebSocketAuther_hpp__
