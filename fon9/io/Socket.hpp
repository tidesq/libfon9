/// \file fon9/io/Socket.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_Socket_hpp__
#define __fon9_io_Socket_hpp__
#include "fon9/io/SocketConfig.hpp"
#include "fon9/Fdr.hpp"
#include "fon9/Outcome.hpp"

fon9_BEFORE_INCLUDE_STD;
#ifdef fon9_WINDOWS
#include <Ws2tcpip.h>
#include <MSWSock.h>
/// \ingroup io
/// 將 dat, datsz 填入 WSABUF;
inline void fon9_PutIoVectorElement(WSABUF* piov, void* dat, size_t datsz) {
   piov->len = static_cast<ULONG>(datsz);
   piov->buf = reinterpret_cast<CHAR*>(dat);
}
#endif
fon9_AFTER_INCLUDE_STD;

//--------------------------------------------------------------------------//

namespace fon9 { namespace io {

using SocketErrC = ErrC;

#ifdef fon9_WINDOWS
inline SocketErrC GetSocketErrC(int eno = WSAGetLastError()) {
   return GetSysErrC(static_cast<DWORD>(eno));
}
fon9_API extern LPFN_CONNECTEX  FnConnectEx;
#else
inline SocketErrC GetSocketErrC(int eno = errno) {
   return GetSysErrC(eno);
}
#endif

/// \ingroup io
/// 用此傳回 socket 操作的結果, 並提供發生錯誤的系統呼叫函式名稱。
class SocketResult : public Result2T<SocketErrC> {
   using base = Result2T<SocketErrC>;
   template <size_t sz> SocketResult(char (&fnName)[sz], const SocketErrC&) = delete;
   template <size_t sz> SocketResult(char (&fnName)[sz], SocketErrC&&) = delete;
public:
   using base::base;
   SocketResult() = default;
   SocketResult(std::nullptr_t) {}

   template <size_t sz>
   SocketResult(const char (&fnName)[sz], SocketErrC&& errc = GetSocketErrC()) : base{fnName, errc} {
   }

   template <size_t sz>
   SocketResult(const char (&fnName)[sz], const SocketErrC& errc) : base{fnName, errc} {
   }
};

//--------------------------------------------------------------------------//

enum class SocketType {
   /// stream socket.
   Stream = SOCK_STREAM,
   /// datagram socket.
   Dgram = SOCK_DGRAM,
   /// raw-protocol interface.
   Raw = SOCK_RAW,
   /// reliably-delivered message.
   RDM = SOCK_RDM,
   /// sequenced packet stream.
   SeqPacket = SOCK_SEQPACKET,
};

fon9_WARN_DISABLE_PADDING;
/// \ingroup io
/// Socket handle.
class fon9_API Socket final {
   fon9_NON_COPYABLE(Socket);
   FdrAuto  So_;
public:

#ifdef fon9_WINDOWS
   using socket_t = SOCKET;
   static constexpr SOCKET kInvalidValue = INVALID_SOCKET;
   static_assert(reinterpret_cast<Fdr::fdr_t>(INVALID_SOCKET) == Fdr::kInvalidValue,
                 "Socket, Fdr 的 kInvalidValue 不一致?! 不能用 FdrAuto So_; 必須另外想辦法處理了!");

   SocketResult CreateOverlappedSocket(AddressFamily family, SocketType type);
   SocketResult CreateDeviceSocket(AddressFamily family, SocketType type) {
      return this->CreateOverlappedSocket(family, type);
   }
#else
   using socket_t = Fdr::fdr_t;
   enum : socket_t { kInvalidValue = Fdr::kInvalidValue };
   SocketResult CreateNonBlockSocket(AddressFamily family, SocketType type);
   SocketResult CreateDeviceSocket(AddressFamily family, SocketType type) {
      return this->CreateNonBlockSocket(family, type);
   }
   explicit Socket(socket_t fd) : So_{fd} {
   }
   bool SetNonBlock() {
      return this->So_.SetNonBlock();
   }
#endif

   Socket();
   Socket(Socket&&) = default;
   Socket& operator=(Socket&&) = default;

   ~Socket() {
      this->Close();
   }

   void Close() {
   #ifdef fon9_WINDOWS
      SOCKET so = reinterpret_cast<SOCKET>(this->So_.ReleaseFD());
      if (so != kInvalidValue)
         ::closesocket(so);
   #else
      this->So_.Close();
   #endif
   }

   bool IsSocketReady() const {
      return this->So_.IsReadyFD();
   }

   /// 透過 getsockopt(SO_ERROR) 取得 socket 的錯誤碼.
   static SocketErrC LoadSocketErrC(socket_t so);
   SocketErrC LoadSocketErrC() const {
      return Socket::LoadSocketErrC(this->GetSocketHandle());
   }

   SocketResult SetSocketOptions(const SocketOptions& opts) const;
   SocketResult Bind(const SocketAddress& addr) const;

#ifdef fon9_WINDOWS
   SOCKET GetSocketHandle() const {
      return reinterpret_cast<SOCKET>(this->So_.GetFD());
   }
#else
   Fdr::fdr_t GetSocketHandle() const {
      return this->So_.GetFD();
   }
#endif
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_io_Socket_hpp__
