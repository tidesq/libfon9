/// \file fon9/io/win/IocpTcpServer.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_win_IocpTcpServer_hpp__
#define __fon9_io_win_IocpTcpServer_hpp__
#include "fon9/io/win/IocpSocket.hpp"
#include "fon9/io/TcpServerBase.hpp"

namespace fon9 { namespace io {

class IocpTcpListener;

/// \ingroup io
/// Windows TcpServer 使用 IOCP
using IocpTcpServer = TcpServerBase<IocpTcpListener, IocpServiceSP>;
using IocpTcpServerSP = DeviceSPT<IocpTcpServer>;

class fon9_API IocpTcpListener : public intrusive_ref_counter<IocpTcpListener>, public IocpHandler {
   fon9_NON_COPY_NON_MOVE(IocpTcpListener);
   class AcceptedClient;

   bool           IsDisposing_{false};

   /// (LocalAddressLength+16) + (RemoteAddressLength+16)
   /// +16 是 Windows 的規定:
   /// This value must be at least 16 bytes more than the maximum address length for the transport protocol in use.
   char           AcceptBuffer_[(sizeof(SocketAddress) + 16) * 2 + 3];

   WSAOVERLAPPED  ListenOverlapped_;
   Socket         ListenSocket_;
   Socket         ClientSocket_;

   using AcceptedClientId = uint64_t;
   using AcceptedClientsImpl = std::vector<AcceptedClient*>;
   static AcceptedClientsImpl::iterator FindAcceptedClients(AcceptedClientsImpl& devs, AcceptedClientId id);

   using AcceptedClients = MustLock<AcceptedClientsImpl>;
   mutable AcceptedClients  AcceptedClients_;
   
   void RemoveAcceptedClient(AcceptedClient& dev);

   virtual void OnIocp_Error(OVERLAPPED* lpOverlapped, DWORD eno) override;
   virtual void OnIocp_Done(OVERLAPPED* lpOverlapped, DWORD bytesTransfered) override;
   void ResetupAccepter();
   bool SetupAccepter(SocketResult& soRes);
   DeviceSP GetAcceptedClient(StrView* acceptedClientId);

   IocpTcpListener(IocpServiceSP&& iosv, IocpTcpServerSP&& server, Socket&& soListen);

public:
   const IocpTcpServerSP   Server_;
   ~IocpTcpListener() {
      this->Dispose("Listener.Dtor");
   }

   using ListenerSP = intrusive_ptr<IocpTcpListener>;
   static ListenerSP CreateListener(IocpTcpServerSP server, SocketResult& soRes);

   void Dispose(std::string cause);

   size_t GetCurrentConnectionCount() const {
      AcceptedClients::Locker devs{this->AcceptedClients_};
      return devs->size();
   }

   void OpImpl_CloseAcceptedClient(StrView acceptedClientId);
   void OpImpl_LingerCloseAcceptedClient(StrView acceptedClientId);
};

} } // namespaces
#endif//__fon9_io_win_IocpTcpServer_hpp__
