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

   typedef bool (*FnOpImpl_IsSocketAlive)(Device& owner, IocpSocket* impl);

   void StartRecv(RecvBufferSize preallocSize);
   void ContinueRecv(RecvBufferSize expectSize) {
      this->RecvBuffer_.SetContinueRecv();
      this->StartRecv(expectSize);
   }
   void InvokeRecvEvent(Device& dev, DcQueueList& rxbuf, FnOpImpl_IsSocketAlive);

   SendBuffer& GetSendBuffer() {
      return this->SendBuffer_;
   }
   void ContinueSend(Device* dev, DWORD bytesTransfered, FnOpImpl_IsSocketAlive);
   Device::SendResult SendAfterAddRef(DcQueueList& dcbuf);

   struct SendChecker {
      IocpSocket*    IocpSocket_{nullptr};
      DcQueueList*   ToSending_{nullptr};

      virtual void PushTo(BufferList&) = 0;
      virtual IocpSocket* OpImpl_GetIocpSocket(Device& dev) = 0;

      std::errc CheckSend(Device& dev);
   };
   struct SendCheckerMem : public SendChecker {
      const void* Mem_;
      size_t      Size_;
      SendCheckerMem(const void* src, size_t size) : Mem_{src}, Size_{size} {
      }
      virtual void PushTo(BufferList& buf) override {
         AppendToBuffer(buf, this->Mem_, this->Size_);
      }
      Device::SendResult Send(Device& dev, DcQueueList* buffered) {
         if (fon9_UNLIKELY(this->Size_ <= 0))
            return Device::SendResult{0};
         std::errc err;
         if ((err = this->CheckSend(dev)) == std::errc{}) {
            if (this->ToSending_) {
               this->ToSending_->Append(this->Mem_, this->Size_);
               return this->IocpSocket_->SendAfterAddRef(buffered ? *buffered : *this->ToSending_);
            }
            return Device::SendResult{0};
         }
         return err;
      }
   };
   struct SendCheckerBuf : public SendChecker {
      BufferList* Src_;
      SendCheckerBuf(BufferList* src) : Src_{src} {
      }
      virtual void PushTo(BufferList& buf) override {
         buf.push_back(std::move(*this->Src_));
      }
      Device::SendResult Send(Device& dev, DcQueueList* buffered) {
         std::errc err;
         if ((err = this->CheckSend(dev)) == std::errc{}) {
            if (this->ToSending_) {
               this->ToSending_->push_back(std::move(*this->Src_));
               return this->IocpSocket_->SendAfterAddRef(buffered ? *buffered : *this->ToSending_);
            }
            return Device::SendResult{0};
         }
         return err;
      }
   };
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_io_win_IocpSocket_hpp__
