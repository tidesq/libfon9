/// \file fon9/io/win/IocpTcpClient.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_win_IocpTcpClient_hpp__
#define __fon9_io_win_IocpTcpClient_hpp__
#include "fon9/io/win/IocpSocket.hpp"
#include "fon9/io/TcpClientBase.hpp"

namespace fon9 { namespace io {

fon9_WARN_DISABLE_PADDING;
struct fon9_API IocpTcpClientImpl : public IocpSocket, public intrusive_ref_counter<IocpTcpClientImpl> {
   fon9_NON_COPY_NON_MOVE(IocpTcpClientImpl);
   bool  IsClosing_{false};
   bool  IsConnected_{false};

   virtual unsigned IocpSocketAddRef() override;
   virtual unsigned IocpSocketReleaseRef() override;

public:
   using OwnerDevice = TcpClientT<IocpServiceSP, IocpTcpClientImpl>;
   using OwnerDeviceSP = intrusive_ptr<OwnerDevice>;
   const OwnerDeviceSP  Owner_;

   IocpTcpClientImpl(OwnerDevice* owner, Socket&& so, SocketResult& soRes)
      : IocpSocket(owner->IoService_, std::move(so), soRes)
      , Owner_{owner} {
   }

   bool IsClosing() const {
      return this->IsClosing_;
   }
   void OpImpl_Close();
   bool OpImpl_ConnectTo(const SocketAddress& addr, SocketResult& soRes);

   virtual void OnIocpSocket_Received(DcQueueList& rxbuf) override;
   virtual void OnIocpSocket_Writable(DWORD bytesTransfered) override;
   virtual void OnIocpSocket_Error(OVERLAPPED* lpOverlapped, DWORD eno) override;

   struct ContinueSendAux : public IocpContinueSendAux {
      using IocpContinueSendAux::IocpContinueSendAux;
      ContinueSendAux() = delete;
      static bool IsBufferAlive(Device& dev, IoBuffer* iobuf) {
         return OwnerDevice::OpImpl_IsBufferAlive(dev, iobuf);
      }
   };
};
fon9_WARN_POP;

//--------------------------------------------------------------------------//

class IocpTcpClientDeviceBase : public IocpTcpClientImpl::OwnerDevice {
   fon9_NON_COPY_NON_MOVE(IocpTcpClientDeviceBase);
   using base = IocpTcpClientImpl::OwnerDevice;

public:
   using base::base;

   template <class SendAuxBase>
   struct SendAux : public SendAuxBase {
      using SendAuxBase::SendAuxBase;
      SendAux() = delete;

      static IocpSocket* GetImpl(IocpTcpClientDeviceBase& dev) {
         return dev.ImplSP_.get();
      }
      void AsyncSend(IocpTcpClientDeviceBase& dev, SendChecker& sc, ObjHolderPtr<BufferList>&& pbuf) {
         sc.AsyncSend(std::move(pbuf), dev.ImplSP_.get(), &OpImpl_IsBufferAlive);
      }
   };
};

/// \ingroup io
/// Windows TcpClient Device 使用 IOCP.
using IocpTcpClient = DeviceImpl_IoBufferSend<IocpTcpClientDeviceBase, IocpSocket>;

} } // namespaces
#endif//__fon9_io_win_IocpTcpClient_hpp__
