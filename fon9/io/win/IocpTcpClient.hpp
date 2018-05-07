/// \file fon9/io/win/IocpTcpClient.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_win_IocpTcpClient_hpp__
#define __fon9_io_win_IocpTcpClient_hpp__
#include "fon9/io/win/IocpSocket.hpp"
#include "fon9/io/TcpClientBase.hpp"

namespace fon9 { namespace io {

fon9_WARN_DISABLE_PADDING;
/// \ingroup io
/// Windows TcpClient 使用 IOCP
class fon9_API IocpTcpClient : public TcpClientBase {
   fon9_NON_COPY_NON_MOVE(IocpTcpClient);
   using base = TcpClientBase;

protected:
   using TcpClientSP = intrusive_ptr<IocpTcpClient>;
   struct IocpClient : public IocpSocket, public intrusive_ref_counter<IocpClient> {
      fon9_NON_COPY_NON_MOVE(IocpClient);
      bool              IsClosing_{false};
      bool              IsConnected_{false};
      const TcpClientSP Owner_;
      IocpClient(IocpTcpClient* owner, Socket&& so, SocketResult& soRes)
         : IocpSocket(owner->IocpService_, std::move(so), soRes)
         , Owner_{owner} {
      }
      void OpImpl_Close();
      bool OpImpl_ConnectTo(const SocketAddress& addr, SocketResult& soRes);

      virtual void OnIocpSocket_Received(DcQueueList& rxbuf) override;
      virtual void OnIocpSocket_Writable(DWORD bytesTransfered) override;
      virtual void OnIocpSocket_Error(OVERLAPPED* lpOverlapped, DWORD eno) override;

      virtual unsigned IocpSocketAddRef() override;
      virtual unsigned IocpSocketReleaseRef() override;
      static bool OpImpl_IsSocketAlive(Device& dev, IocpSocket* impl);
   };
   using IocpClientSP = intrusive_ptr<IocpClient>;
   IocpClientSP   ImplSP_;

   void OpImpl_ResetImplSP() {
      if (IocpClientSP impl = std::move(this->ImplSP_))
         impl->OpImpl_Close();
   }

   virtual void OpImpl_TcpLinkBroken() override;
   virtual void OpImpl_TcpClearLinking() override;
   virtual bool OpImpl_TcpConnect(Socket&& soCli, SocketResult& soRes) override;
   virtual void OpImpl_StartRecv(RecvBufferSize preallocSize) override;

   template <class SendCheckerBase>
   struct SendChecker : public SendCheckerBase {
      SendChecker() = delete;
      using SendCheckerBase::SendCheckerBase;
      virtual IocpSocket* OpImpl_GetIocpSocket(Device& dev) override {
         return static_cast<IocpTcpClient*>(&dev)->ImplSP_.get();
      }
   };
   using SendCheckerMem = SendChecker<IocpSocket::SendCheckerMem>;
   using SendCheckerBuf = SendChecker<IocpSocket::SendCheckerBuf>;
public:
   const IocpServiceSP  IocpService_;
   IocpTcpClient(IocpServiceSP iosv, SessionSP ses, ManagerSP mgr)
      : TcpClientBase(std::move(ses), std::move(mgr))
      , IocpService_{std::move(iosv)} {
   }

   virtual bool IsSendBufferEmpty() const override;
   virtual SendResult SendASAP(const void* src, size_t size) override;
   virtual SendResult SendASAP(BufferList&& src) override;
   virtual SendResult SendBuffered(const void* src, size_t size) override;
   virtual SendResult SendBuffered(BufferList&& src) override;
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_io_win_IocpTcpClient_hpp__
