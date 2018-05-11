/// \file fon9/io/win/IocpSocket.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_win_IocpSocket_hpp__
#define __fon9_io_win_IocpSocket_hpp__
#include "fon9/io/win/IocpService.hpp"
#include "fon9/io/Socket.hpp"
#include "fon9/io/IoBufferSend.hpp"

namespace fon9 { namespace io {

fon9_WARN_DISABLE_PADDING;
fon9_MSC_WARN_DISABLE_NO_PUSH(4623); /* 'IocpSendASAP_AuxMem' : default constructor was implicitly defined as deleted */
/// \ingroup io
/// 使用 Overlapped I/O 建議 SNDBUF=0, 可以讓 WSASend(1024*1024*100) 更快返回.
/// | SNDBUF |      txBytes  | WSASend() elapsed(ms) |
/// |-------:|--------------:|-----------------------|
/// |  10240 | 1024*1024*100 | 57, 33, 30, 28...     |
/// |      0 |             0 | 33,  3,  3,  3...     |
class fon9_API IocpSocket : public IoBuffer, public IocpHandler {
   fon9_NON_COPY_NON_MOVE(IocpSocket);
   bool DropRecv();

protected:
   WSAOVERLAPPED     SendOverlapped_;
   WSAOVERLAPPED     RecvOverlapped_;
   DWORD             Eno_{0}; // Eno_ 通常在 OnIocp_Error() 時設定, 在解構時清除 SendBuffer_ 使用.

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

   virtual void StartRecv(RecvBufferSize preallocSize) override;

   void StartSend(DcQueueList& toSend) {
      this->IocpSocketAddRef();
      this->SendAfterAddRef(toSend);
   }
   Device::SendResult SendAfterAddRef(DcQueueList& dcbuf);

   //--------------------------------------------------------------------------//

   struct SendAux_ImplProtector {
      SendAux_ImplProtector(IocpSocket* impl) {
         impl->IocpSocketAddRef();
      }
   };

   struct SendASAP_AuxMem : public SendAuxMem {
      using SendAuxMem::SendAuxMem;
      using ImplProtector = SendAux_ImplProtector;

      Device::SendResult Send(Device&, IocpSocket& impl, DcQueueList& toSend) {
         toSend.Append(this->Src_, this->Size_);
         return impl.SendAfterAddRef(toSend);
      }
   };

   struct SendASAP_AuxBuf : public SendAuxBuf {
      using SendAuxBuf::SendAuxBuf;
      using ImplProtector = SendAux_ImplProtector;

      Device::SendResult Send(Device&, IocpSocket& impl, DcQueueList& toSend) {
         toSend.push_back(std::move(*this->Src_));
         return impl.SendAfterAddRef(toSend);
      }
   };

   struct SendBuffered_AuxMem : public SendAuxMem {
      using SendAuxMem::SendAuxMem;
      using ImplProtector = SendAux_ImplProtector;

      Device::SendResult Send(Device&, IocpSocket& impl, DcQueueList& toSend) {
         toSend.Append(this->Src_, this->Size_);
         DcQueueList emptyBuffer;
         return impl.SendAfterAddRef(emptyBuffer);
      }
   };

   struct SendBuffered_AuxBuf : public SendAuxBuf {
      using SendAuxBuf::SendAuxBuf;
      using ImplProtector = SendAux_ImplProtector;

      Device::SendResult Send(Device&, IocpSocket& impl, DcQueueList& toSend) {
         toSend.push_back(std::move(*this->Src_));
         DcQueueList emptyBuffer;
         return impl.SendAfterAddRef(emptyBuffer);
      }
   };

   //--------------------------------------------------------------------------//

   struct IocpContinueSendAux {
      DWORD BytesTransfered_;
      IocpContinueSendAux(DWORD bytesTransfered) : BytesTransfered_(bytesTransfered) {
      }
      DcQueueList* GetToSend(SendBuffer& sbuf) const {
         return sbuf.OpImpl_ContinueSend(this->BytesTransfered_);
      }
      static void DisableWritableEvent(IoBuffer&) {
         // IOCP socket 透過 WSASend() 啟動「一次」Writable, 所以不用額外取消 writable.
      }
      void StartSend(Device&, IoBuffer& iobuf, DcQueueList& toSend) const {
         static_cast<IocpSocket*>(&iobuf)->StartSend(toSend);
      }
   };
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_io_win_IocpSocket_hpp__
