// \file fon9/web/WebSocket.cpp
// \author fonwinz@gmail.com
#include "fon9/web/WebSocket.hpp"
#include "fon9/web/HttpDate.hpp"
#include "fon9/crypto/Sha1.hpp"
#include "fon9/Base64.hpp"
#include "fon9/RevPrint.hpp"

namespace fon9 { namespace web {

enum : size_t {
   kWebSocketMaxPayloadSize = 1024 * 1024 * 8,
};

fon9_API bool IsUpgradeToWebSocket(const HttpMessage& msg) {
   return iequals(msg.FindHeadField("upgrade"), "websocket");
}
fon9_API io::RecvBufferSize OnBadWebSocketRequest(io::Device& dev, HttpRequest& req) {
   HttpHandler::SendErrorPrefix(dev, req, "400 Bad Request", RevBufferList{128});
   dev.AsyncClose("Bad WebSocket request.");
   return io::RecvBufferSize::NoLink;
}
fon9_API RevBufferList UpgradeToWebSocket(io::Device& dev, HttpRequest& req) {
   // "sec-websocket-version" == 13?
   StrView wkey = req.Message_.FindHeadField("sec-websocket-key");
   if (wkey.empty()) {
      OnBadWebSocketRequest(dev, req);
      return RevBufferList{0};
   }
   crypto::ContextSha1 sha1;
   sha1.Init();
   sha1.Update(wkey.begin(), wkey.size());
   static const char magicString[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
   sha1.Update(magicString, sizeof(magicString) - 1);
   byte bSecWsAccept[crypto::Sha1::kOutputSize];
   sha1.Final(bSecWsAccept);
   char sSecWsAccept[Base64EncodeLengthNoEOS(sizeof(bSecWsAccept))];
   Base64Encode(sSecWsAccept, sizeof(sSecWsAccept), bSecWsAccept, sizeof(bSecWsAccept));

   RevBufferList rbuf{128};
   RevPrint(rbuf, fon9_kCSTR_HTTPCRLN2);// header tail.
   RevPutMem(rbuf, sSecWsAccept, sizeof(sSecWsAccept));
   RevPrint(rbuf, "Upgrade: websocket"  fon9_kCSTR_HTTPCRLN
            "Connection: Upgrade" fon9_kCSTR_HTTPCRLN
            "Sec-WebSocket-Accept: ");
   return rbuf;
}

fon9_API io::RecvBufferSize SendWebSocketAccepted(io::Device& dev, RevBufferList&& rbuf) {
   RevPrint(rbuf, fon9_kCSTR_HTTP11 " 101 Switching Protocols" fon9_kCSTR_HTTPCRLN
            "Date: ", FmtHttpDate{UtcNow()}, fon9_kCSTR_HTTPCRLN);
   dev.Send(rbuf.MoveOut());
   return io::RecvBufferSize::NoLink;
}

//--------------------------------------------------------------------------//

void WebSocket::Send(WebSocketOpCode opCode, RevBufferList&& rbuf, bool isFIN) {
   byte frameHeader[sizeof(this->FrameHeader_)];
   frameHeader[0] = static_cast<byte>(opCode);
   if (isFIN)
      frameHeader[0] |= static_cast<byte>(0x80);
   size_t bufsz = CalcDataSize(rbuf.cfront());
   if (bufsz <= 125) {
      frameHeader[1] = static_cast<byte>(bufsz);
      RevPutMem(rbuf, frameHeader, 2);
   }
   else if (bufsz <= 0xffff) {
      frameHeader[1] = 126;
      PutBigEndian(frameHeader + 2, static_cast<uint16_t>(bufsz));
      RevPutMem(rbuf, frameHeader, 2 + sizeof(uint16_t));
   }
   else {
      frameHeader[1] = 127;
      PutBigEndian(frameHeader + 2, static_cast<uint64_t>(bufsz));
      RevPutMem(rbuf, frameHeader, 2 + sizeof(uint64_t));
   }
   // server has no MASK.
   this->Device_->Send(rbuf.MoveOut());
}
void WebSocket::Send(WebSocketOpCode opCode, const void* buf, size_t bufsz, bool isFIN) {
   RevBufferList rbuf{sizeof(this->FrameHeader_)};
   RevPutMem(rbuf, buf, bufsz);
   return this->Send(opCode, std::move(rbuf), isFIN);
}

//--------------------------------------------------------------------------//

static inline bool hasMask(const byte* pHeader) {
   return ((pHeader[1] & 0x80) != 0);
}
static inline byte getPayloadLenId(const byte* pHeader) {
   return static_cast<byte>(pHeader[1] & 0x7f);
}

bool WebSocket::PeekFrameHeaderLen(DcQueueList& rxbuf) {
   // https://developer.mozilla.org/zh-CN/docs/Web/API/WebSockets_API/Writing_WebSocket_servers
   // https://tools.ietf.org/html/rfc6455#section-5.1
   //  Frame format:
   //  0                   1                   2                   3
   //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   //  +-+-+-+-+-------+-+-------------+-------------------------------+
   //  |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
   //  |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
   //  |N|V|V|V|       |S|             |   (if payload len==126/127)   |
   //  | |1|2|3|       |K|             |                               |
   //  +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
   //  |     Extended payload length continued, if payload len == 127  |
   //  + - - - - - - - - - - - - - - - +-------------------------------+
   //  |                               |Masking-key, if MASK set to 1  |
   //  +-------------------------------+-------------------------------+
   //  | Masking-key (continued)       |          Payload Data         |
   //  +-------------------------------- - - - - - - - - - - - - - - - +
   //  :                     Payload Data continued ...                :
   //  + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
   //  |                     Payload Data continued ...                |
   //  +---------------------------------------------------------------+
   assert(this->Stage_ == Stage::WaittingFrameHeader);
   const byte* pHeader = reinterpret_cast<const byte*>(rxbuf.Peek(this->FrameHeader_, 2));
   if (!pHeader)
      return false;

   // https://tools.ietf.org/html/rfc6455#section-5.1
   // if(!hasMASK) at server must close connection.
   // if(hasMASK)  at client must close connection.
   size_t   headerSize = (hasMask(pHeader) ? 6u : 2u);
   uint64_t payloadLenId = getPayloadLenId(pHeader);
   if (payloadLenId == 126)
      headerSize += 2;
   else if (payloadLenId == 127)
      headerSize += 8;
   else
      this->RemainPayloadLen_ = payloadLenId;
   this->FrameHeaderLen_ = static_cast<uint8_t>(headerSize);
   this->Stage_ = (this->FrameHeaderLen_ == 2 ? Stage::FrameHeaderReady : Stage::FrameHeaderLenReady);
   return true;
}
bool WebSocket::FetchFrameHeader(DcQueueList& rxbuf) {
   assert(this->Stage_ == Stage::FrameHeaderLenReady);
   assert(2 <= this->FrameHeaderLen_ && this->FrameHeaderLen_ <= sizeof(this->FrameHeader_));
   if (rxbuf.Fetch(this->FrameHeader_, this->FrameHeaderLen_) == 0)
      return false;
   uint64_t payloadLen = getPayloadLenId(this->FrameHeader_);
   if (payloadLen == 126)
      this->RemainPayloadLen_ = GetBigEndian<uint16_t>(this->FrameHeader_ + 2);
   else if (payloadLen == 127)
      this->RemainPayloadLen_ = GetBigEndian<uint64_t>(this->FrameHeader_ + 2);
   this->Stage_ = Stage::FrameHeaderReady;
   if (this->RemainPayloadLen_ + this->Payload_.size() > kWebSocketMaxPayloadSize) {
      this->Device_->AsyncClose("Payload size too big: > kWebSocketMaxPayloadSize");
      return false;
   }
   return true;
}
io::RecvBufferSize WebSocket::FetchPayload(DcQueueList& rxbuf) {
   assert(this->Stage_ == Stage::FrameHeaderReady);
   if (this->RemainPayloadLen_ > 0) {
      if (rxbuf.CalcSize() < this->RemainPayloadLen_)
         return io::RecvBufferSize::AsyncRecvEvent;
      size_t rdfrom = this->Payload_.size();
      this->Payload_.resize(rdfrom + this->RemainPayloadLen_);
      char*  pfrom = &this->Payload_[rdfrom];
      rxbuf.Read(pfrom, this->RemainPayloadLen_);
      if (hasMask(this->FrameHeader_)) {
         const byte* pmask = this->FrameHeader_ + this->FrameHeaderLen_ - 4;
         for (size_t i = 0; i < this->RemainPayloadLen_; ++i) {
            *pfrom = static_cast<char>(*pfrom ^ pmask[i & 0x03]);
            ++pfrom;
         }
      }
      this->RemainPayloadLen_ = 0;
   }
   this->FrameHeaderLen_ = 0;
   this->Stage_ = Stage::WaittingFrameHeader;
   if ((this->FrameHeader_[0] & 0x80) == 0) // FIN = 0, 還沒收完, 應接續下一個 frame.
      return io::RecvBufferSize::Default;
   io::RecvBufferSize retval = io::RecvBufferSize::Default;
   switch (static_cast<WebSocketOpCode>(this->FrameHeader_[0] & 0x0f)) {
   case WebSocketOpCode::ContinueFrame: // 已在 FIN=0 時返回, 如果執行到這, 表示 frame 不符規則.
   case WebSocketOpCode::TextFrame:
   case WebSocketOpCode::BinaryFrame:
      retval = this->OnWebSocketMessage();
      break;
   case WebSocketOpCode::Ping:
      this->Send(WebSocketOpCode::Pong, this->Payload_.c_str(), this->Payload_.size());
      break;
   case WebSocketOpCode::ConnectionClose:
      this->Device_->AsyncClose("WebSocket: OpCode.ConnectionClose");
   case WebSocketOpCode::Pong:
      break;
   }
   this->Payload_.clear();
   return retval;
}
io::RecvBufferSize WebSocket::OnDevice_Recv(io::Device& dev, DcQueueList& rxbuf) {
   (void)dev; assert(this->Device_ == &dev);
   for (;;) {
      switch (this->Stage_) {
      case Stage::WaittingFrameHeader:
         if (this->PeekFrameHeaderLen(rxbuf))
            continue;
         break;
      case Stage::FrameHeaderLenReady:
         if (this->FetchFrameHeader(rxbuf))
            continue;
         break;
      case Stage::FrameHeaderReady:
         io::RecvBufferSize retval = this->FetchPayload(rxbuf);
         if(retval >= io::RecvBufferSize::Default)
            continue;
         return retval == io::RecvBufferSize::AsyncRecvEvent ? io::RecvBufferSize::Default : retval;
      }
      return io::RecvBufferSize::Default;
   }
}

} } // namespaces
