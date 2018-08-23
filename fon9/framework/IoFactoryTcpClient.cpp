/// \file fon9/framework/IoFactoryTcpClient.cpp
/// \author fonwinz@gmail.com
#include "fon9/framework/IoManager.hpp"

#ifdef fon9_WINDOWS
#include "fon9/io/win/IocpTcpClient.hpp"
static fon9::io::IocpTcpClient* CreateDevice(fon9::IoManager& mgr, fon9::io::SessionSP&& ses) {
   return new fon9::io::IocpTcpClient(mgr.GetIocpService(), std::move(ses), &mgr);
}
#else
#include "fon9/io/FdrTcpClient.hpp"
static fon9::io::FdrTcpClient* CreateDevice(fon9::IoManager& mgr, fon9::io::SessionSP&& ses) {
   return new fon9::io::FdrTcpClient(mgr.GetFdrService(), std::move(ses), &mgr);
}
#endif

namespace fon9 {

fon9_API DeviceFactorySP GetIoFactoryTcpClient() {
   struct Factory : public DeviceFactory {
      fon9_NON_COPY_NON_MOVE(Factory);
      Factory() : DeviceFactory("TcpClient") {
      }
      io::DeviceSP CreateDevice(IoManagerSP mgr, SessionFactory& sesFactory, const IoConfigItem& cfg) override {
         auto ses = sesFactory.CreateSession(*mgr, cfg);
         if (!ses)
            return io::DeviceSP{};
         return ::CreateDevice(*mgr, std::move(ses));
      }
   };
   return new Factory;
}

} // namespaces
