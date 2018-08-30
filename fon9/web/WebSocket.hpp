// \file fon9/web/WebSocket.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_web_WebSocket_hpp__
#define __fon9_web_WebSocket_hpp__
#include "fon9/web/HttpSession.hpp"

namespace fon9 { namespace web {

enum class WebSocketOpCode : byte {
   ContinueFrame = 0x0,
   TextFrame = 0x1,
   BinaryFrame = 0x2,
   // 0x3 - 0x7 are reserved for further non - control frames

   ConnectionClose = 0x8,
   Ping = 0x9,
   Pong = 0xA,
   // 0xB - 0xF are reserved for further control frames
};

/// \ingroup web
/// WebSocket 訊息接收及解析, 解析由衍生者 override OnWebSocketMessage() 處理.
/// \code
/// class MyHttpHandler : public fon9::web::HttpHandler {
///   fon9::io::RecvBufferSize OnHttpRequest(fon9::io::Device& dev, fon9::web::HttpRequest& req) override {
///      if (req.MessageSt_ != fon9::web::HttpMessageSt::FullMessage) // 必須等收完 request 訊息才能 upgrade ws.
///         return fon9::io::RecvBufferSize::Default;
///      if (fon9::web::IsUpgradeToWebSocket(req.Message_)) { // 如果是 upgrade ws request, 則執行底下步驟升級.
///         fon9::RevBufferList rbuf = fon9::web::UpgradeToWebSocket(dev, req); // request 訊息必須正確.
///         if (rbuf.cfront()) { // 如果 request 訊息正確, 則 rbuf 會包含回覆的必要 header.
///            // 確定升級, 則建立您自己的 WebSocket 服務物件: new MyWebSocket;
///            // 並通知 HttpSession
///            static_cast<fon9::web::HttpSession*>(dev.Session_.get())->UpgradeTo(fon9::web::WebSocketSP{new MyWebSocket{&dev}});
///            // 您可以在 rbuf 前, 加上自已定義的 header, 然後送出同意升級訊息.
///            // 如果引有升級後的立即訊息要送, 則必須先送升級訊息, 然後返回 fon9::io::RecvBufferSize::NoLink;
///            fon9::web::SendWebSocketAccepted(dev, std::move(rbuf));
///            // 這裡可以送出升級後的立即訊息.
///            return fon9::io::RecvBufferSize::NoLink
///         }
///      }
///      // 如果除了 upgrade ws 的訊息, 其他都是 BadRequest, 則透過底下函式送出拒絕 & 斷線.
///      // 如果允許非 upgrade 訊息, 則可自行處理其他 request.
///      return fon9::web::OnBadWebSocketRequest(dev, req);
///   }
/// };
/// \endcode
class fon9_API WebSocket : public HttpRecvHandler {
   fon9_NON_COPY_NON_MOVE(WebSocket);
protected:
   const io::DeviceSP Device_;
   enum class Stage : byte {
      WaittingFrameHeader = 0,
      FrameHeaderLenReady,
      FrameHeaderReady,
   }           Stage_{};
   uint8_t     FrameHeaderLen_;
   byte        FrameHeader_[14];
   uint64_t    RemainPayloadLen_{0};
   std::string Payload_;

   bool PeekFrameHeaderLen(DcQueueList& rxbuf);
   bool FetchFrameHeader(DcQueueList& rxbuf);
   io::RecvBufferSize FetchPayload(DcQueueList& rxbuf);

   virtual io::RecvBufferSize OnWebSocketMessage() = 0;
public:
   WebSocket(io::DeviceSP dev) : Device_{std::move(dev)} {
   }
   io::RecvBufferSize OnDevice_Recv(io::Device& dev, DcQueueList& rxbuf) override;

   void Send(WebSocketOpCode opCode, RevBufferList&& rbuf, bool isFIN = true);
   void Send(WebSocketOpCode opCode, const void* buf, size_t bufsz, bool isFIN = true);
   void Send(WebSocketOpCode opCode, StrView msg, bool isFIN = true) {
      return this->Send(opCode, msg.begin(), msg.size(), isFIN);
   }
};
using WebSocketSP = std::unique_ptr<WebSocket>;

/// \ingroup web
/// return iequals(msg.FindHeadField("upgrade"), "websocket");
fon9_API bool IsUpgradeToWebSocket(const HttpMessage& msg);

/// \ingroup web
/// 送出 "400 Bad Request", 並關閉連線.
fon9_API io::RecvBufferSize OnBadWebSocketRequest(io::Device& dev, HttpRequest& req);

/// \ingroup web
/// 如果失敗(req header 有誤), 則 retval.cfront()==nullptr;
/// 否則傳回值包含部分必要的 header, 您可以繼續增加其他 header,
/// 然後透過 SendWebSocketAccepted() 送出同意訊息.
fon9_API RevBufferList UpgradeToWebSocket(io::Device& dev, HttpRequest& req);

/// \ingroup web
/// rbuf = 經過 UpgradeToWebSocket() 處理過後的 WebSocket Upgrade 回覆訊息.
/// 透過這裡加上最終的 header 後送出同意訊息.
/// return fon9::io::RecvBufferSize::NoLink;
fon9_API io::RecvBufferSize SendWebSocketAccepted(io::Device& dev, RevBufferList&& rbuf);

} } // namespaces
#endif//__fon9_web_WebSocket_hpp__
