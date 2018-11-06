/// \file fon9/io/Socket.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/Socket.hpp"
#include "fon9/StrTo.hpp"

fon9_BEFORE_INCLUDE_STD;
#ifdef fon9_WINDOWS
#include <mstcpip.h>//SIO_KEEPALIVE_VALS
#else
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <signal.h>
/// 將 size_t size 轉成 static_cast<socklen_t>(size) 避免警告.
#define inet_ntop(af,src,dst,size)  inet_ntop(af, src, dst, static_cast<socklen_t>(size))
#endif
fon9_AFTER_INCLUDE_STD;

namespace fon9 { namespace io {

#ifdef fon9_WINDOWS
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")
fon9_API LPFN_CONNECTEX  FnConnectEx;
struct SockInit {
   SockInit() {
      WSADATA wsaData;
      WSAStartup(MAKEWORD(2, 2), &wsaData);

      GUID   guidConnectEx = WSAID_CONNECTEX;
      DWORD  resBytes;
      SOCKET so = socket(AF_INET, SOCK_DGRAM, 0);
      ::WSAIoctl(so, SIO_GET_EXTENSION_FUNCTION_POINTER,
                 &guidConnectEx, sizeof(guidConnectEx),
                 &FnConnectEx, sizeof(FnConnectEx),
                 &resBytes, NULL, NULL);
      closesocket(so);
   }
   ~SockInit() {
      WSACleanup();
   }
};
static SockInit SockInit_;

bool Socket::CreateOverlappedSocket(AddressFamily family, SocketType type, SocketResult& soRes) {
   this->So_.SetFD(reinterpret_cast<Fdr::fdr_t>(WSASocketW(static_cast<int>(family), static_cast<int>(type), 0, NULL, 0, WSA_FLAG_OVERLAPPED)));
   if (!this->So_.IsReadyFD()) {
      soRes = SocketResult{"WSASocketW"};
      return false;
   }
   // 為何要設定 FIONBIO?
   // (1) 讓 DropRecv() 裡面的 WSARecv() 不會阻塞.
   // (2) 底下的想法太天真 => 因為在 WSA_FLAG_OVERLAPPED 情況下, WSASend(無 overlapped)、send() 似乎仍會 block?!!
   //    => 因為使用 WSASend(overlapped) 若有立即複製到 SNDBUF, 仍必須在 IocpService thread 處理送出結果.
   //    => 這裡設定成 FIONBIO, 則可以先不使用 overlapped, 直到發生 EWOULDBLOCK(或 EAGAIN) 的錯誤時, 才使用 overlapped.
   //    => 這樣可以避免 SendASAP() 之後, IopcService thread 處理結果之前, 其他 SendASAP() 都變成 SendBuffer();
   //    若呼叫 send(1024 * 1024 * 100), 不論有無 FIONBIO, 返回值都是 1024 * 1024 * 100:
   //    | FIONBIO | SNDBUF | send() elapsed(ms) |
   //    |:-------:|-------:|--------------------|
   //    |    Y    |  10240 |  60,  30...        |
   //    |    N    |  10240 |  63,  30...        |
   //    |    Y    |      0 | 341, 248...        |
   //    |    N    |      0 | 291, 177...        |
   unsigned long nNonBlocking = 1;
   if (ioctlsocket(this->GetSocketHandle(), FIONBIO, &nNonBlocking) == SOCKET_ERROR) {
      soRes = SocketResult{"FIONBIO"};
      return false;
   }
   return true;
}
#else
struct SockInit {
   SockInit() {
      ::signal(SIGPIPE, SIG_IGN);
   }
};
static SockInit SockInit_;

bool Socket::CreateNonBlockSocket(AddressFamily family, SocketType type, SocketResult& soRes) {
   int   sockType = static_cast<int>(type);
   #ifdef SOCK_NONBLOCK
      sockType |= SOCK_NONBLOCK;
   #endif
   #ifdef SOCK_CLOEXEC
      sockType |= SOCK_CLOEXEC;
   #endif
   this->So_.SetFD(::socket(static_cast<int>(family), sockType, 0)); // IPPROTO_TCP
   if (!this->So_.IsReadyFD()) {
      soRes = SocketResult{"socket"};
      return false;
   }
   #ifndef SOCK_NONBLOCK
   if (!this->So_.SetNonBlock()) {
      soRes = SocketResult{"SetNonBlock"};
      return false;
   }
   #endif
   return true;
}
#endif

//--------------------------------------------------------------------------//

Socket::Socket() {
   // Socket建構, 與 SockInit_ 放在同一個.cpp檔案裡面, 確保 SockInit_ 可以先執行.
}

int Socket::LoadSocketErrno(socket_t so) {
   int         eno;
   socklen_t   len = sizeof(eno);
   getsockopt(so, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&eno), &len);
   return eno;
}

bool Socket::Bind(const SocketAddress& addr, SocketResult& soRes) const {
   if (::bind(this->GetSocketHandle(), &addr.Addr_, addr.GetAddrLen()) == 0)
      return true;
   soRes = SocketResult{"bind"};
   return false;
}

//--------------------------------------------------------------------------//

template <size_t optNameSize>
inline static void SetSocketResultError(const char (&cstrOptName)[optNameSize], SocketResult& soRes) {
   if (soRes) // 使用第一個錯誤訊息: 尚未設定錯誤, 才需要設定.
      soRes = SocketResult{cstrOptName};
}

template <typename socket_t, typename ValueT, size_t optNameSize>
inline static void SetOpt(socket_t so, int level, int optname, const ValueT& value, const char (&cstrOptName)[optNameSize], SocketResult& soRes) {
   if (::setsockopt(so, level, optname, reinterpret_cast<const char*>(&value), sizeof(value)) != 0)
      SetSocketResultError(cstrOptName, soRes);
}

bool Socket::SetSocketOptions(const SocketOptions& opts, SocketResult& soRes) const {
   auto  so = this->GetSocketHandle();
#ifdef SO_NOSIGPIPE // iOS
   int   setNoSigPIPE = 1;
   setsockopt(so, SOL_SOCKET, SO_NOSIGPIPE, &setNoSigPIPE, "NoSigPIPE", sizeof(setNoSigPIPE));
#endif
   if (opts.TCP_NODELAY_)
      SetOpt(so, IPPROTO_TCP, TCP_NODELAY, opts.TCP_NODELAY_, "TcpNoDelay", soRes);
   if (opts.SO_SNDBUF_ >= 0)
      SetOpt(so, SOL_SOCKET, SO_SNDBUF, opts.SO_SNDBUF_, "SNDBUF", soRes);
   if (opts.SO_RCVBUF_ >= 0)
      SetOpt(so, SOL_SOCKET, SO_RCVBUF, opts.SO_RCVBUF_, "RCVBUF", soRes);
   if (opts.SO_REUSEADDR_)
      SetOpt(so, SOL_SOCKET, SO_REUSEADDR, opts.SO_REUSEADDR_, "ReuseAddr", soRes);
#ifdef SO_REUSEPORT
   if (opts.SO_REUSEPORT_)
      SetOpt(so, SOL_SOCKET, SO_REUSEPORT, opts.SO_REUSEPORT_, "ReusePort", soRes);
#endif
   if (opts.Linger_.l_onoff || opts.Linger_.l_linger)
      SetOpt(so, SOL_SOCKET, SO_LINGER, opts.Linger_, "Linger", soRes);

   if (opts.KeepAliveInterval_) {
      if (opts.KeepAliveInterval_ == 1)
         SetOpt(so, SOL_SOCKET, SO_KEEPALIVE, opts.KeepAliveInterval_, "KeepAlive.ON", soRes);
      else {
      #ifdef fon9_WINDOWS
         tcp_keepalive   tcpKeepAlive;
         unsigned long   ulBytesReturn = 0;
         tcpKeepAlive.onoff = true;
         // keepalivetime member specifies the timeout, in milliseconds,
         // with no activity until the first keep-alive packet is sent.
         tcpKeepAlive.keepalivetime = static_cast<ULONG>(opts.KeepAliveInterval_) * 1000;
         // keepaliveinterval member specifies the interval, in milliseconds,
         // between when successive keep - alive packets are sent if no acknowledgement is received.
         tcpKeepAlive.keepaliveinterval = tcpKeepAlive.keepalivetime;
         if (WSAIoctl(so, SIO_KEEPALIVE_VALS, &tcpKeepAlive, sizeof(tcpKeepAlive), nullptr, 0, &ulBytesReturn, NULL, NULL) == SOCKET_ERROR)
            SetSocketResultError("KeepAlive.VALS", soRes);
      #else
         int   keepAlive = true;
         SetOpt(so, SOL_SOCKET, SO_KEEPALIVE, keepAlive, "KeepAlive.ON", soRes);
         #ifdef TCP_KEEPALIVE_THRESHOLD//SunOS
         #   define TCP_KEEPIDLE  TCP_KEEPALIVE_THRESHOLD
         #   define TCP_KEEPINTVL TCP_KEEPALIVE_ABORT_THRESHOLD
         #endif
         #ifdef TCP_KEEPIDLE
            SetOpt(so, IPPROTO_TCP, TCP_KEEPIDLE, opts.KeepAliveInterval_, "KeepAlive.Idle", soRes);
         #endif
         #ifdef TCP_KEEPINTVL
            SetOpt(so, IPPROTO_TCP, TCP_KEEPINTVL, opts.KeepAliveInterval_, "KeepAlive.Interval", soRes);
         #endif
         #if defined(IPPROTO_TCP) && defined(TCP_KEEPCNT)
            int   keepCount = 3;
            SetOpt(so, IPPROTO_TCP, TCP_KEEPCNT, keepCount, "KeepAlive.KeepCnt", soRes);
         #endif
      #endif
      }
   }
   return soRes.IsSuccess();
}

} } // namespaces
