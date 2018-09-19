/// \file fon9/framework/IoFactoryTcpServer.cpp
/// \author fonwinz@gmail.com
#include "fon9/framework/IoManager.hpp"

#ifdef fon9_WINDOWS
#include "fon9/io/win/IocpTcpServer.hpp"
static fon9::io::IocpTcpServer* CreateDevice(fon9::IoManager& mgr, fon9::io::SessionServerSP&& ses) {
   return new fon9::io::IocpTcpServer(mgr.GetIocpService(), std::move(ses), &mgr);
}
#else
#include "fon9/io/FdrTcpServer.hpp"
static fon9::io::FdrTcpServer* CreateDevice(fon9::IoManager& mgr, fon9::io::SessionServerSP&& ses) {
   return new fon9::io::FdrTcpServer(mgr.GetFdrService(), std::move(ses), &mgr);
}
#endif

namespace fon9 {

fon9_API DeviceFactorySP GetIoFactoryTcpServer() {
   struct Factory : public DeviceFactory {
      fon9_NON_COPY_NON_MOVE(Factory);
      Factory() : DeviceFactory("TcpServer") {
      }
      io::DeviceSP CreateDevice(IoManagerSP mgr, SessionFactory& sesFactory, const IoConfigItem& cfg, std::string& errReason) override {
         if (auto ses = sesFactory.CreateSessionServer(*mgr, cfg, errReason))
            return ::CreateDevice(*mgr, std::move(ses));
         return io::DeviceSP{};
      }
   };
   return new Factory;
}

} // namespaces
