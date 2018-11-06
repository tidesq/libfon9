/// \file fon9/io/DgramBase.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_DgramBase_hpp__
#define __fon9_io_DgramBase_hpp__
#include "fon9/io/SocketClientDevice.hpp"

namespace fon9 { namespace io {

fon9_WARN_DISABLE_PADDING;
/// \ingroup io
/// Dgram 基底, 衍生出: IocpDgram, FdrDgram;
/// Dgram 包含 Udp, Multicast.
class fon9_API DgramBase : public SocketClientDevice {
   fon9_NON_COPY_NON_MOVE(DgramBase);
   DgramBase() = delete;
   using base = SocketClientDevice;

   SocketAddress  Group_;
   SocketAddress  Interface_;
   int            Loopback_;
   uint8_t        TTL_;

protected:
   void OpImpl_Open(std::string cfgstr) override;
   ConfigParser::Result OpImpl_SetProperty(StrView tag, StrView& value) override;
   bool CreateSocket(Socket& so, const SocketAddress& addr, SocketResult& soRes) override;
   void OpImpl_Connected(Socket::socket_t so);
   void OpImpl_OnAddrListEmpty() override;

public:
   DgramBase(SessionSP ses, ManagerSP mgr)
      : base(std::move(ses), std::move(mgr), Style::Client) {
   }
};
fon9_WARN_POP;

template <class IoServiceSP, class ClientImplT>
using DgramT = SocketClientDeviceT<IoServiceSP, ClientImplT, DgramBase>;

} } // namespaces
#endif//__fon9_io_DgramBase_hpp__
