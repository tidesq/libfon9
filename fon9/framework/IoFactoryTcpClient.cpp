/// \file fon9/framework/IoFactoryTcpClient.cpp
/// \author fonwinz@gmail.com
#include "fon9/framework/NamedIoManager.hpp"

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

fon9_API DeviceFactorySP MakeIoFactoryTcpClient(std::string name) {
   struct Factory : public DeviceFactory {
      fon9_NON_COPY_NON_MOVE(Factory);
      Factory(std::string name) : DeviceFactory(std::move(name)) {
      }
      io::DeviceSP CreateDevice(IoManagerSP mgr, SessionFactory& sesFactory, const IoConfigItem& cfg, std::string& errReason) override {
         if (auto ses = sesFactory.CreateSession(*mgr, cfg, errReason))
            return ::CreateDevice(*mgr, std::move(ses));
         return io::DeviceSP{};
      }
   };
   return new Factory(name);
}
static bool TcpClient_Start(seed::PluginsHolder& holder, StrView args) {
   struct ArgsParser : public DeviceFactoryConfigParser {
      ArgsParser() : DeviceFactoryConfigParser{"TcpClient"} {}
      DeviceFactorySP CreateDeviceFactory() override {
         return MakeIoFactoryTcpClient(this->Name_);
      }
   };
   return ArgsParser{}.Parse(holder, args);
}

} // namespaces

extern "C" fon9_API fon9::seed::PluginsDesc f9p_TcpClient;
static fon9::seed::PluginsPark f9pRegister{"TcpClient", &f9p_TcpClient};

fon9::seed::PluginsDesc f9p_TcpClient{
   "",
   &fon9::TcpClient_Start,
   nullptr,
   nullptr,
};
