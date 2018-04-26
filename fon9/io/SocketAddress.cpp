/// \file fon9/io/SocketAddress.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/SocketAddress.hpp"
#include "fon9/StrTo.hpp"
#include "fon9/ToStr.hpp"

fon9_BEFORE_INCLUDE_STD;
#ifdef fon9_WINDOWS
#include <Ws2tcpip.h>
#else
#include <arpa/inet.h>
// 將 size_t size 轉成 static_cast<socklen_t>(size) 避免警告.
#define inet_ntop(af,src,dst,size)  inet_ntop(af, src, dst, static_cast<socklen_t>(size))
#endif
fon9_AFTER_INCLUDE_STD;

namespace fon9 { namespace io {

bool SocketAddress::IsEmpty() const {
   return this->Addr_.sa_family == AF_UNSPEC
      || (this->Addr_.sa_family == AF_INET6
          && this->Addr6_.sin6_port == 0
          && memcmp(&this->Addr6_.sin6_addr, &in6addr_any, sizeof(in6addr_any)) == 0)
      || (this->Addr_.sa_family == AF_INET
          && this->Addr4_.sin_port == 0
          && this->Addr4_.sin_addr.s_addr == htonl(INADDR_ANY));
}

bool SocketAddress::IsAddrAny() const {
   return this->Addr_.sa_family == AF_INET6
      ? memcmp(&this->Addr6_.sin6_addr, &in6addr_any, sizeof(in6addr_any)) == 0
      : this->Addr4_.sin_addr.s_addr == htonl(INADDR_ANY);
}

bool SocketAddress::operator==(const SocketAddress& rhs) const {
   return this->Addr_.sa_family == AF_INET6
      ? (this->Addr6_.sin6_port == rhs.Addr6_.sin6_port && memcmp(&this->Addr6_.sin6_addr, &rhs.Addr6_.sin6_addr, sizeof(rhs.Addr6_.sin6_addr)) == 0)
      : (this->Addr4_.sin_port == rhs.Addr4_.sin_port && this->Addr4_.sin_addr.s_addr == rhs.Addr4_.sin_addr.s_addr);
}

void SocketAddress::SetAddrAny(AddressFamily af, port_t port) {
   if ((this->Addr_.sa_family = static_cast<sa_family_t>(af)) == AF_INET6) {
      this->Addr6_.sin6_addr = in6addr_any;
      this->Addr6_.sin6_port = htons(port);
   }
   else {
      this->Addr4_.sin_addr.s_addr = htonl(INADDR_ANY);
      this->Addr4_.sin_port = htons(port);
   }
}

//--------------------------------------------------------------------------//

void SocketAddress::FromStr(StrView strAddr) {
   char  strbuf[1024];
   if (StrTrimHead(&strAddr).Get1st() == '[') {
      this->Addr_.sa_family = AF_INET6;
      strAddr.SetBegin(strAddr.begin() + 1);
      StrView addr6 = StrFetchTrim(strAddr, ']');
      if (!inet_pton(AF_INET6, addr6.ToString(strbuf), &this->Addr6_.sin6_addr))
         this->Addr6_.sin6_addr = in6addr_any;
      // IPv6 addr port 可接受底下2種格式:
      //    [addr]:port
      //    [addr]port
      if (StrTrimHead(&strAddr).Get1st() == ':')
         strAddr.SetBegin(strAddr.begin() + 1);
      this->Addr6_.sin6_port = htons(StrTo(strAddr, port_t{}));
   }
   else {
      this->Addr_.sa_family = AF_INET;
      StrView strPort;
      if (const char* pPort = StrView::traits_type::find(strAddr.begin(), strAddr.size(), ':')) {
         // addr:port
         strPort.Reset(pPort + 1, strAddr.end());
         strAddr.SetEnd(StrFindTrimTail(strAddr.begin(), pPort));
      }
      else {
         // 沒找到 ':', 可能 「只有addr」 或 「只有 port」.
         if (StrView::traits_type::find(strAddr.begin(), strAddr.size(), '.'))
            StrTrimTail(&strAddr);
         else {
            // 沒有'.'(沒有addr) => 表示只有 port.
            strPort = strAddr;
            strAddr.SetBegin(strPort.end());
         }
      }
      if (strAddr.empty() || !inet_pton(AF_INET, strAddr.ToString(strbuf), &this->Addr4_.sin_addr))
         this->Addr4_.sin_addr.s_addr = htonl(INADDR_ANY);
      this->Addr4_.sin_port = (strPort.empty() ? static_cast<port_t>(0) : htons(StrTo(strPort, port_t{})));
   }
}

//--------------------------------------------------------------------------//

size_t SocketAddress::ToAddrStr(char strbuf[], size_t bufsz) const {
   if (fon9_UNLIKELY(bufsz <= 3)) {
      if (bufsz > 0)
         strbuf[0] = 0;
      return 0;
   }
   const void* paddr;
   char* pstr;
   if (this->Addr_.sa_family == AF_INET6) {
      pstr = strbuf;
      paddr = &this->Addr6_.sin6_addr;
      --bufsz;
      *pstr++ = '[';
      *pstr = 0;
      inet_ntop(this->Addr_.sa_family, const_cast<void*>(paddr), pstr, bufsz);
      pstr = strchr(pstr, 0);
      if (static_cast<size_t>(pstr - strbuf) < bufsz) {
         *pstr++ = ']';
         *pstr = 0;
      }
   }
   else {
      strbuf[0] = 0;
      paddr = &this->Addr4_.sin_addr;
      inet_ntop(AF_INET, const_cast<void*>(paddr), strbuf, bufsz);
      pstr = strchr(strbuf, 0);
   }
   return static_cast<size_t>(pstr - strbuf);
}

size_t SocketAddress::ToAddrPortStr(char strbuf[], size_t bufsz) const {
   char*       pAddrEnd = strbuf + this->ToAddrStr(strbuf, bufsz);
   char* const pBuffEOS = strbuf + bufsz - 1;
   if (fon9_LIKELY(pAddrEnd + 1 < pBuffEOS)) {
      *pAddrEnd++ = ':';
      std::ptrdiff_t bufRemain = (pBuffEOS - pAddrEnd);
      if (fon9_LIKELY(bufRemain > 0)) {
         uintmax_t   port = (this->Addr_.sa_family == AF_INET6)
            ? ntohs(this->Addr6_.sin6_port)
            : ntohs(this->Addr4_.sin_port);
         NumOutBuf nbuf;
         char*     pout = UIntToStrRev(nbuf.end(), port);
         size_t    outLen = nbuf.GetLength(pout);
         if (fon9_LIKELY(static_cast<size_t>(bufRemain) >= outLen)) {
            memcpy(pAddrEnd, pout, outLen);
            pAddrEnd += outLen;
         }
         else
            *pAddrEnd++ = '#';
      }
      *pAddrEnd = 0;
   }
   return static_cast<size_t>(pAddrEnd - strbuf);
}

//--------------------------------------------------------------------------//

StrView MakeTcpConnectionUID(char* const uid, size_t uidSize, const SocketAddress* addrRemote, const SocketAddress* addrLocal) {
   if (fon9_UNLIKELY(uidSize <= 3)) {
      if (uidSize > 0)
         uid[0] = 0;
      return StrView{};
   }
   char* puid = uid;
   char* pend = puid + uidSize;
   if (addrRemote) {
      *puid++ = '|';
      *puid++ = 'R';
      *puid++ = '=';
      puid += addrRemote->ToAddrPortStr(puid, static_cast<size_t>(pend - puid));
   }
   if (addrLocal && pend - puid > 3) {
      *puid++ = '|';
      *puid++ = 'L';
      *puid++ = '=';
      puid += addrLocal->ToAddrPortStr(puid, static_cast<size_t>(pend - puid));
   }
   *puid = 0;
   return StrView{uid, puid};
}

} } // namespaces
