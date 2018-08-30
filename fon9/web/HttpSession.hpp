// \file fon9/web/HttpSession.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_web_HttpSession_hpp__
#define __fon9_web_HttpSession_hpp__
#include "fon9/web/HttpHandler.hpp"
#include "fon9/framework/IoFactory.hpp"

namespace fon9 { namespace web {

/// \ingroup web
/// Http byte stream 接收基底.
/// 實際的訊息解析由衍生者(HttpMessageReceiver, WebSocket)負責.
class fon9_API HttpRecvHandler {
public:
   virtual ~HttpRecvHandler();
   virtual io::RecvBufferSize OnDevice_Recv(io::Device& dev, DcQueueList& rxbuf) = 0;
};
using HttpRecvHandlerSP = std::unique_ptr<HttpRecvHandler>;

/// \ingroup web
/// Http Message 訊息接收及解析, 解析後交給 HttpHandler 處理.
class fon9_API HttpMessageReceiver : public HttpRecvHandler {
   fon9_NON_COPY_NON_MOVE(HttpMessageReceiver);
   HttpRequest           Request_;
   const HttpHandlerSP   RootHandler_;

   /// 在底下3種情況觸發:
   /// - HttpResult::Incomplete && this->Request_.Message_.IsHeaderReady() && !this->Request_.IsHeaderReady()
   /// - HttpResult::FullMessage
   /// - HttpResult::ChunkAppended
   io::RecvBufferSize OnRequestEvent(io::Device& dev);
public:
   HttpMessageReceiver(HttpHandlerSP rootHandler) : RootHandler_{std::move(rootHandler)} {
   }
   io::RecvBufferSize OnDevice_Recv(io::Device& dev, DcQueueList& rxbuf) override;
};
using HttpMessageReceiverSP = std::unique_ptr<HttpMessageReceiver>;

//--------------------------------------------------------------------------//

/// \ingroup web
/// 一個簡易的 http server.
/// - 一開始透過 HttpMessageReceiver.OnDevice_Recv() 處理.
/// - 一旦升級為 WebSocket 連線, 會轉給 WebSocket.OnDevice_Recv() 處理.
class fon9_API HttpSession : public io::Session {
   fon9_NON_COPY_NON_MOVE(HttpSession);
   class Server;

protected:
   HttpRecvHandlerSP RecvHandler_;
   io::RecvBufferSize OnDevice_Recv(io::Device& dev, DcQueueList& rxbuf) override;
   void OnDevice_StateChanged(io::Device& dev, const io::StateChangedArgs& e) override;

public:
   static SessionFactorySP MakeFactory(std::string factoryName, HttpHandlerSP wwwRootHandler);
   HttpSession(HttpHandlerSP rootHandler);
   /// maybe upgrade to WebSocket.
   void UpgradeTo(HttpRecvHandlerSP ws);
};

} } // namespaces
#endif//__fon9_web_HttpSession_hpp__
