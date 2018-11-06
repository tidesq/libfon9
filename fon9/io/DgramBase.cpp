/// \file fon9/io/DgramBase.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/DgramBase.hpp"

namespace fon9 { namespace io {
//
// udp config:
// - 送: RemoteIp:Port
// - 收
//   - 只收特定來源(若有要收 multicast 則為必要):
//       Bind=ListenIp(LocalIp):Port
//   - 可收任意來源:
//       Bind=Port
//   - Linux multicast 只收特定 group, 避免收到其他 group 的訊息:
//       Bind=Group:Port
//
// multicast config:
// - 收:
//    Group=ip|Bind=port
//    Group=ip|Bind=ip:port
//    額外選項:
//       Interface=LocalIp
//       ReuseAddr=Y
//       ReusePort=Y
// - 送:
//    GroupIp:Port
//    額外選項:
//       Interface=LocalIp
//       Loopback=Y or N
//       TTL=hops    必須有提供 Loopback 選項.
//

bool DgramBase::CreateSocket(Socket& so, const SocketAddress& addr, SocketResult& soRes) {
   this->Config_.Options_.TCP_NODELAY_ = 0;
   if (!so.CreateDeviceSocket(addr.GetAF(), SocketType::Dgram, soRes))
      return false;
   // multicast-recv: Join the multicast group
   if (!this->Group_.IsUnspec()) {
      struct ip_mreq  mreq;
      mreq.imr_multiaddr = this->Group_.Addr4_.sin_addr;
      if (this->Interface_.IsUnspec())
         mreq.imr_interface.s_addr = htonl(INADDR_ANY);
      else
         mreq.imr_interface = this->Interface_.Addr4_.sin_addr;
      if (setsockopt(so.GetSocketHandle(), IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<char*>(&mreq), sizeof(mreq)) != 0) {
         soRes = SocketResult{"IP_ADD_MEMBERSHIP"};
         return false;
      }
   }
   // multicast-send: interface
   if (!this->Interface_.IsUnspec()) {
      struct in_addr* pifaddr = &this->Interface_.Addr4_.sin_addr;
      if (setsockopt(so.GetSocketHandle(), IPPROTO_IP, IP_MULTICAST_IF, reinterpret_cast<char*>(pifaddr), sizeof(*pifaddr)) != 0) {
         soRes = SocketResult{"IP_MULTICAST_IF"};
         return false;
      }
   }
   // multicast-send: Loopback
   if (this->Loopback_ >= 0) {
      if (setsockopt(so.GetSocketHandle(), IPPROTO_IP, IP_MULTICAST_LOOP, reinterpret_cast<char*>(&this->Loopback_), sizeof(this->Loopback_)) != 0) {
         soRes = SocketResult{"IP_MULTICAST_LOOP"};
         return false;
      }
      // multicast-send: TTL
      if (this->TTL_ > 0) {
         if (setsockopt(so.GetSocketHandle(), IPPROTO_IP, IP_MULTICAST_TTL, reinterpret_cast<char*>(&this->TTL_), sizeof(this->TTL_)) != 0) {
            soRes = SocketResult{"IP_MULTICAST_TTL"};
            return false;
         }
      }
   }
   else if (this->TTL_ > 0) { // udp 也有 TTL
      if (setsockopt(so.GetSocketHandle(), IPPROTO_IP, IP_TTL, reinterpret_cast<char*>(&this->TTL_), sizeof(this->TTL_)) != 0) {
         soRes = SocketResult{"IP_TTL"};
         return false;
      }
   }
   return true;
}
void DgramBase::OpImpl_Open(std::string cfgstr) {
   this->Group_.Addr_.sa_family = AF_UNSPEC;
   this->Interface_.Addr_.sa_family = AF_UNSPEC;
   this->TTL_ = 0;
   this->Loopback_ = -1;
   base::OpImpl_Open(std::move(cfgstr));
}
ConfigParser::Result DgramBase::OpImpl_SetProperty(StrView tag, StrView& value) {
   if (iequals(tag, "Group") || iequals(tag, "g")) {
      this->Group_.FromStr(value);
      if (this->Config_.AddrBind_.IsEmpty() && this->Group_.GetPort() != 0)
         this->Config_.AddrBind_.SetAddrAny(this->Group_.GetAF(), this->Group_.GetPort());
      return ConfigParser::Result::Success;
   }
   if (iequals(tag, "Interface") || iequals(tag, "i")) {
      this->Interface_.FromStr(value);
      return ConfigParser::Result::Success;
   }
   if (iequals(tag, "TTL")) {
      this->TTL_ = StrTo(value, this->TTL_);
      return ConfigParser::Result::Success;
   }
   if (iequals(tag, "Loopback")) {
      this->Loopback_ = (toupper(value.Get1st()) == 'Y');
      return ConfigParser::Result::Success;
   }
   return base::OpImpl_SetProperty(tag, value);
}

static inline const SocketAddress* GetAddrOrNull(const SocketAddress& addr) {
   return addr.GetPort() == 0 ? nullptr : &addr;
}
void DgramBase::OpImpl_Connected(Socket::socket_t) {
   char     uidbuf[kMaxTcpConnectionUID];
   StrView  uidstr = MakeTcpConnectionUID(uidbuf, GetAddrOrNull(this->RemoteAddress_), GetAddrOrNull(this->Config_.AddrBind_));
   this->OpImpl_SetConnected(uidstr.ToString());
}
void DgramBase::OpImpl_OnAddrListEmpty() {
   this->AddrList_.resize(1);
   this->AddrList_[0].SetAddrAny(AddressFamily::INET4, 0);
   this->OpImpl_ConnectToNext(StrView{});
}

} } // namespaces
