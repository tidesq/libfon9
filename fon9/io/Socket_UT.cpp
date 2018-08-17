/// \file fon9/io/Socket_UT.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/Socket.hpp"
#include "fon9/io/SocketAddressDN.hpp"
#include "fon9/io/SocketServer.hpp"
#include "fon9/io/SocketClient.hpp"
#include "fon9/TestTools.hpp"
#include <thread>

#ifdef fon9_WINDOWS
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#endif

int main() {
#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
   fon9::AutoPrintTestInfo utinfo("Socket");

   std::string dn{"localhost, www.google.com:6666,  localhost:bad,  127.0.0.1:https,aaa,[www.yahoo.com]:5555  "};
   std::cout << "Test dn resolve|dn=" << dn << std::endl;

   fon9::io::DnQueryReqId reqid;
   bool isDone = false;
   unsigned ms = 1;
   do {
      fon9::io::AsyncDnQuery(reqid, dn, 0,
                             [&isDone](fon9::io::DnQueryReqId id, fon9::io::DomainNameParseResult& res) {
         isDone = true;
         std::cout << "|reqid=" << id << "|err=" << res.ErrMsg_ << std::endl;
         for (const auto& addr : res.AddressList_) {
            char ip[fon9::io::SocketAddress::kMaxAddrPortStrSize];
            addr.ToAddrPortStr(ip);
            std::cout << ip << std::endl;
         }
      });
      std::this_thread::sleep_for(std::chrono::milliseconds{ms *= 2});
      fon9::io::AsyncDnQuery_CancelAndWait(&reqid);
      std::cout << '.' << std::flush;
   } while (!isDone);
   std::cout << "[OK   ] Test done|@delay(ms)=" << ms << std::endl;

   //--------------------------------------------------------------------------//
   utinfo.PrintSplitter();
   std::cout << "[TEST ] SocketClientConfig::ParseConfig()";
   fon9::StrView                 res;
   fon9::io::SocketClientConfig  clicfg;
   clicfg.SetDefaults();
   #define cstrDN "www.google.com:https, www.yahoo.com:http, 192.168.1.1:6666"
   struct CliParser : public fon9::io::SocketClientConfig::Parser {
      fon9_NON_COPY_NON_MOVE(CliParser);
      using base = fon9::io::SocketClientConfig::Parser;
      using base::base;
      Result OnTagValue(fon9::StrView tag, fon9::StrView& value) override {
         if (tag == "MyTag" && value == "MyValue")
            return Result::Success;
         return base::OnTagValue(tag, value);
      }
   };
   fon9::StrView cfgstr{"192.168.1.3:5555|Timeout=99|DN=" cstrDN
      "|TcpNoDelay=N|SNDBUF=1234|RCVBUF=5678|ReuseAddr=Y|ReusePort=Y|Linger=N|KeepAlive=8"
      "|MyTag=MyValue|Bind=192.168.1.4:29999"
      "|ERR-TEST"};
   if (CliParser{clicfg}.Parse(cfgstr) != fon9::ConfigParser::Result::EUnknownTag
       || cfgstr != "ERR-TEST") {
      std::cout << "|@:" << cfgstr.ToString() << "\r[ERROR]" << std::endl;
      abort();
   }
   
   //----------------------------------------
   const char* cstrErrFieldName;
   #define CHECK_VALUE(cfg, fld, value) \
      if (cfg.fld != value) { \
         cstrErrFieldName = #fld; \
         goto __ERROR_cstrErrFieldName; \
      }
   //----------------------------------------
   CHECK_VALUE(clicfg, TimeoutSecs_,                99);
   CHECK_VALUE(clicfg, DomainNames_,                cstrDN);
   CHECK_VALUE(clicfg, Options_.TCP_NODELAY_,       0);
   CHECK_VALUE(clicfg, Options_.SO_SNDBUF_,         1234);
   CHECK_VALUE(clicfg, Options_.SO_RCVBUF_,         5678);
   CHECK_VALUE(clicfg, Options_.SO_REUSEADDR_,      1);
   CHECK_VALUE(clicfg, Options_.SO_REUSEPORT_,      1);
   CHECK_VALUE(clicfg, Options_.Linger_.l_onoff,    1);
   CHECK_VALUE(clicfg, Options_.Linger_.l_linger,   0);
   CHECK_VALUE(clicfg, Options_.KeepAliveInterval_, 8);

   if (clicfg.AddrRemote_.Addr_.sa_family != AF_INET
       || clicfg.AddrRemote_.Addr4_.sin_addr.s_addr != 0x0301a8c0
       || clicfg.AddrRemote_.Addr4_.sin_port != htons(5555)) {
      cstrErrFieldName = "AddrRemote_";
__ERROR_cstrErrFieldName:
      std::cout << "|err=" << cstrErrFieldName << "\r[ERROR ]" << std::endl;
      abort();
   }
   if (clicfg.AddrBind_.Addr_.sa_family != AF_INET
       || clicfg.AddrBind_.Addr4_.sin_addr.s_addr != 0x0401a8c0
       || clicfg.AddrBind_.Addr4_.sin_port != htons(29999)) {
      cstrErrFieldName = "AddrBind_";
      goto __ERROR_cstrErrFieldName;
   }
   std::cout << "\r[OK   ]" << std::endl;

   //--------------------------------------------------------------------------//
   utinfo.PrintSplitter();
   std::cout << "[TEST ] SocketServerConfig::ParseConfig()";
   fon9::io::SocketServerConfig  sercfg;
   sercfg.SetDefaults();
   struct SerParser : public fon9::io::SocketServerConfig::Parser {
      fon9_NON_COPY_NON_MOVE(SerParser);
      using base = fon9::io::SocketServerConfig::Parser;
      using base::base;
      Result OnTagValue(fon9::StrView tag, fon9::StrView& value) override {
         if (tag == "MyServerTag" && value == "MyServerValue")
            return Result::Success;
         return base::OnTagValue(tag, value);
      }
      Result OnTagValueClient(fon9::StrView tag, fon9::StrView& value) override {
         if (tag == "MyClientTag" && value == "MyClientValue")
            return Result::Success;
         return base::OnTagValueClient(tag, value);
      }
   };
   cfgstr = "[::1]9999|Remote=[2406:2000:ec:815::3]:8888|ListenBacklog=100"
      "|Capacity=10240|ThreadCount=99|Wait=Busy|Cpus=1,2,3"
      "|ClientOptions="
         "{TcpNoDelay=N|SNDBUF=1234|RCVBUF=5678|ReuseAddr=Y|ReusePort=Y|Linger=N|KeepAlive=8"
         "|MyClientTag=MyClientValue}"
      "|MyServerTag=MyServerValue|ERR-TEST";
   if (SerParser{sercfg}.Parse(cfgstr) != fon9::ConfigParser::Result::EUnknownTag
       || cfgstr != "ERR-TEST") {
      std::cout << "|@:" << cfgstr.ToString() << "\r[ERROR]" << std::endl;
      abort();
   }

   fon9::io::IoServiceArgs::CpuAffinity   cpus{1,2,3};
   CHECK_VALUE(sercfg, ServiceArgs_.CpuAffinity_, cpus);
   CHECK_VALUE(sercfg, ServiceArgs_.ThreadCount_, 99);
   CHECK_VALUE(sercfg, ServiceArgs_.HowWait_,     fon9::io::HowWait::Busy);
   CHECK_VALUE(sercfg, ServiceArgs_.Capacity_,    10240);
   CHECK_VALUE(sercfg, ListenBacklog_, 100);

   struct in6_addr sin6_addr;
   inet_pton(AF_INET6, "2406:2000:ec:815::3", &sin6_addr);
   if (sercfg.ListenConfig_.AddrRemote_.Addr_.sa_family != AF_INET6
       || memcmp(&sercfg.ListenConfig_.AddrRemote_.Addr6_.sin6_addr, &sin6_addr, sizeof(sin6_addr)) != 0
       || sercfg.ListenConfig_.AddrRemote_.Addr6_.sin6_port != htons(8888)) {
      cstrErrFieldName = "ListenConfig_.AddrRemote_";
      goto __ERROR_cstrErrFieldName;
   }
   inet_pton(AF_INET6, "::1", &sin6_addr);
   if (sercfg.ListenConfig_.AddrBind_.Addr_.sa_family != AF_INET6
       || memcmp(&sercfg.ListenConfig_.AddrBind_.Addr6_.sin6_addr, &sin6_addr, sizeof(sin6_addr)) != 0
       || sercfg.ListenConfig_.AddrBind_.Addr6_.sin6_port != htons(9999)) {
      cstrErrFieldName = "ListenConfig_.AddrBind_";
      goto __ERROR_cstrErrFieldName;
   }
   if (memcmp(&sercfg.AcceptedSocketOptions_, &clicfg.Options_, sizeof(clicfg.Options_)) != 0) {
      cstrErrFieldName = "AcceptedClientOptions_";
      goto __ERROR_cstrErrFieldName;
   }
   std::cout << "\r[OK   ]" << std::endl;
}
