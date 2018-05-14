/// \file fon9/io/win/IocpSocket.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_win_IocpSocket_hpp__
#define __fon9_io_win_IocpSocket_hpp__
#include "fon9/io/win/IocpService.hpp"
#include "fon9/io/Socket.hpp"
#include "fon9/io/DeviceStartSend.hpp"
#include "fon9/io/DeviceRecvEvent.hpp"

namespace fon9 { namespace io {

fon9_WARN_DISABLE_PADDING;
fon9_MSC_WARN_DISABLE_NO_PUSH(4623); /* 'IocpSendASAP_AuxMem' : default constructor was implicitly defined as deleted */
/// \ingroup io
/// 使用 Overlapped I/O 建議 SNDBUF=0, 可以讓 WSASend(1024*1024*100) 更快返回.
/// | SNDBUF |      txBytes  | WSASend() elapsed(ms) |
/// |-------:|--------------:|-----------------------|
/// |  10240 | 1024*1024*100 | 57, 33, 30, 28...     |
/// |      0 |             0 | 33,  3,  3,  3...     |
class fon9_API IocpSocket : public IocpHandler {
   fon9_NON_COPY_NON_MOVE(IocpSocket);
   bool DropRecv();

protected:
   WSAOVERLAPPED  SendOverlapped_;
   SendBuffer     SendBuffer_;

   WSAOVERLAPPED  RecvOverlapped_;
   RecvBuffer     RecvBuffer_;

   DWORD          Eno_{0}; // Eno_ 通常在 OnIocp_Error() 時設定, 在解構時清除 SendBuffer_ 使用.

   virtual void OnIocp_Error(OVERLAPPED* lpOverlapped, DWORD eno) override;
   virtual void OnIocp_Done(OVERLAPPED* lpOverlapped, DWORD bytesTransfered) override;
   /// 返回前必須主動再次呼叫 StartRecv();
   virtual void OnIocpSocket_Received(DcQueueList& rxbuf) = 0;
   virtual void OnIocpSocket_Writable(DWORD bytesTransfered) = 0;
   virtual void OnIocpSocket_Error(OVERLAPPED* lpOverlapped, DWORD eno) = 0;

public:
   const Socket   Socket_;

   IocpSocket(IocpServiceSP iosv, Socket&& so, SocketResult& soRes);
   ~IocpSocket();

   virtual unsigned IocpSocketAddRef() = 0;
   virtual unsigned IocpSocketReleaseRef() = 0;

   StrView GetOverlappedKind(OVERLAPPED* lpOverlapped) const {
      return lpOverlapped == &this->SendOverlapped_ ? StrView{"Send"}
         : lpOverlapped == &this->RecvOverlapped_ ? StrView{"Recv"}
      : StrView{"Unknown"};
   }
   std::string GetErrorMessage(OVERLAPPED* lpOverlapped, DWORD eno) const;

   //--------------------------------------------------------------------------//

   void StartRecv(RecvBufferSize preallocSize);
   void ContinueRecv(RecvBufferSize expectSize) {
      this->StartRecv(expectSize);
   }
   RecvBuffer& GetRecvBuffer() {
      return this->RecvBuffer_;
   }
   struct IocpRecvAux {
      static void ContinueRecv(RecvBuffer& rbuf, RecvBufferSize expectSize) {
         ContainerOf(rbuf, &IocpSocket::RecvBuffer_).ContinueRecv(expectSize);
      }
      static void DisableReadableEvent(RecvBuffer&) {
         // IOCP socket 透過 WSARecv() 啟動「一次」readable, 所以不用額外取消 readable.
      }
   };

   //--------------------------------------------------------------------------//

   SendBuffer& GetSendBuffer() {
      return this->SendBuffer_;
   }
   void StartContinueSend(DcQueueList& toSend) {
      this->IocpSocketAddRef();
      this->SendAfterAddRef(toSend);
   }
   Device::SendResult SendAfterAddRef(DcQueueList& dcbuf);

   struct SendAux_SendBufferProtector {
      SendAux_SendBufferProtector(SendBuffer& sbuf) {
         IocpSocket& impl = ContainerOf(sbuf, &IocpSocket::SendBuffer_);
         impl.IocpSocketAddRef();
      }
   };

   struct SendASAP_AuxMem : public SendAuxMem {
      using SendAuxMem::SendAuxMem;
      using SendBufferProtector = SendAux_SendBufferProtector;

      Device::SendResult StartSend(Device&, DcQueueList& toSend) {
         toSend.Append(this->Src_, this->Size_);
         IocpSocket& impl = ContainerOf(SendBuffer::StaticCast(toSend), &IocpSocket::SendBuffer_);
         return impl.SendAfterAddRef(toSend);
      }
   };

   struct SendASAP_AuxBuf : public SendAuxBuf {
      using SendAuxBuf::SendAuxBuf;
      using SendBufferProtector = SendAux_SendBufferProtector;

      Device::SendResult StartSend(Device&, DcQueueList& toSend) {
         toSend.push_back(std::move(*this->Src_));
         IocpSocket& impl = ContainerOf(SendBuffer::StaticCast(toSend), &IocpSocket::SendBuffer_);
         return impl.SendAfterAddRef(toSend);
      }
   };

   struct SendBuffered_AuxMem : public SendAuxMem {
      using SendAuxMem::SendAuxMem;
      using SendBufferProtector = SendAux_SendBufferProtector;

      Device::SendResult StartSend(Device&, DcQueueList& toSend) {
         toSend.Append(this->Src_, this->Size_);
         DcQueueList emptyBuffer;
         IocpSocket& impl = ContainerOf(SendBuffer::StaticCast(toSend), &IocpSocket::SendBuffer_);
         return impl.SendAfterAddRef(emptyBuffer);
      }
   };

   struct SendBuffered_AuxBuf : public SendAuxBuf {
      using SendAuxBuf::SendAuxBuf;
      using SendBufferProtector = SendAux_SendBufferProtector;

      Device::SendResult StartSend(Device&, DcQueueList& toSend) {
         toSend.push_back(std::move(*this->Src_));
         DcQueueList emptyBuffer;
         IocpSocket& impl = ContainerOf(SendBuffer::StaticCast(toSend), &IocpSocket::SendBuffer_);
         return impl.SendAfterAddRef(emptyBuffer);
      }
   };

   //--------------------------------------------------------------------------//

   struct ContinueSendAux {
      DWORD BytesTransfered_;
      ContinueSendAux(DWORD bytesTransfered) : BytesTransfered_(bytesTransfered) {
      }
      DcQueueList* GetToSend(SendBuffer& sbuf) const {
         return sbuf.OpImpl_ContinueSend(this->BytesTransfered_);
      }
      static void DisableWritableEvent(SendBuffer&) {
         // IOCP socket 透過 WSASend() 啟動「一次」writable, 所以不用額外取消 writable.
      }
      void StartContinueSend(Device&, DcQueueList& toSend) const {
         IocpSocket& impl = ContainerOf(SendBuffer::StaticCast(toSend), &IocpSocket::SendBuffer_);
         impl.StartContinueSend(toSend);
      }
   };
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_io_win_IocpSocket_hpp__
