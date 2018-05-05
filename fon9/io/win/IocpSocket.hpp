/// \file fon9/io/win/IocpSocket.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_win_IocpSocket_hpp__
#define __fon9_io_win_IocpSocket_hpp__
#include "fon9/io/win/IocpService.hpp"
#include "fon9/io/SendBuffer.hpp"
#include "fon9/io/RecvBuffer.hpp"
#include "fon9/io/Socket.hpp"

namespace fon9 { namespace io {

fon9_WARN_DISABLE_PADDING;
/// \ingroup io
/// 使用 Overlapped I/O 建議 SNDBUF=0, 可以讓 WSASend(1024*1024*100) 更快返回.
/// | SNDBUF |      txBytes  | WSASend() elapsed(ms) |
/// |-------:|--------------:|-----------------------|
/// |  10240 | 1024*1024*100 | 57, 33, 30, 28...     |
/// |      0 |             0 | 33,  3,  3,  3...     |
class IocpSocket : public IocpHandler {
   fon9_NON_COPY_NON_MOVE(IocpSocket);
   bool DropRecv();

protected:
   WSAOVERLAPPED     SendOverlapped_;
   SendBuffer        SendBuffer_;
   WSAOVERLAPPED     RecvOverlapped_;
   RecvBuffer        RecvBuffer_;
   DWORD             Eno_{0}; // Eno_ 通常在 OnIocp_Error() 時設定, 在解構時清除 SendBuffer_ 使用.

   virtual void OnIocp_Error(OVERLAPPED* lpOverlapped, DWORD eno) override;
   virtual void OnIocp_Done(OVERLAPPED* lpOverlapped, DWORD bytesTransfered) override;
   /// 返回前必須主動再次呼叫 StartRecv();
   virtual void OnIocpSocket_Recv(DcQueueList& rxbuf) = 0;
   virtual void OnIocpSocket_Error(OVERLAPPED* lpOverlapped, DWORD eno) = 0;
   virtual void OnIocpSocket_Writable(DWORD bytesTransfered) = 0;

public:
   const Socket   Socket_;

   IocpSocket(IocpServiceSP iosv, Socket&& so)
      : IocpHandler{std::move(iosv)}
      , Socket_{std::move(so)} {
   }
   ~IocpSocket();

   StrView GetOverlappedKind(OVERLAPPED* lpOverlapped) const {
      return lpOverlapped == &this->SendOverlapped_ ? StrView{"Send"}
         : lpOverlapped == &this->RecvOverlapped_ ? StrView{"Recv"}
         : StrView{"Unknown"};
   }

   bool IsSendBufferEmpty() const {
      return this->SendBuffer_.IsEmpty();
   }
   SendBuffer& GetSendBuffer() {
      return this->SendBuffer_;
   }

   void StartRecv(RecvBufferSize preallocSize);
   void ContinueRecv(RecvBufferSize expectSize) {
      this->RecvBuffer_.SetContinueRecv();
      this->StartRecv(expectSize);
   }
   virtual unsigned AddRef() = 0;
   virtual unsigned ReleaseRef() = 0;

   Device::SendResult SendAfterAddRef(DcQueueList& dcbuf);
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_io_win_IocpSocket_hpp__
