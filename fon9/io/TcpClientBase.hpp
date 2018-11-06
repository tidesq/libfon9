/// \file fon9/io/TcpClientBase.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_TcpClientBase_hpp__
#define __fon9_io_TcpClientBase_hpp__
#include "fon9/io/SocketClientDevice.hpp"

namespace fon9 { namespace io {

/// \ingroup io
/// TcpClient 基底, 衍生者: IocpTcpClient, FdrTcpClient;
class fon9_API TcpClientBase : public SocketClientDevice {
   fon9_NON_COPY_NON_MOVE(TcpClientBase);
   TcpClientBase() = delete;
   using base = SocketClientDevice;

protected:
   bool CreateSocket(Socket& so, const SocketAddress& addr, SocketResult& soRes) override;
   void OpImpl_Connected(Socket::socket_t so);

public:
   TcpClientBase(SessionSP ses, ManagerSP mgr)
      : base(std::move(ses), std::move(mgr), Style::Client) {
   }
};

template <class IoServiceSP, class ClientImplT>
using TcpClientT = SocketClientDeviceT<IoServiceSP, ClientImplT, TcpClientBase>;

} } // namespaces
#endif//__fon9_io_TcpClientBase_hpp__
