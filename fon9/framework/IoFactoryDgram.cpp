/// \file fon9/framework/IoFactoryDgram.cpp
/// \author fonwinz@gmail.com
#include "fon9/framework/IoManager.hpp"

#ifdef fon9_WINDOWS
#include "fon9/io/win/IocpDgram.hpp"
static fon9::io::IocpDgram* CreateDevice(fon9::IoManager& mgr, fon9::io::SessionSP&& ses) {
   return new fon9::io::IocpDgram(mgr.GetIocpService(), std::move(ses), &mgr);
}
#else
#include "fon9/io/FdrDgram.hpp"
static fon9::io::FdrDgram* CreateDevice(fon9::IoManager& mgr, fon9::io::SessionSP&& ses) {
   return new fon9::io::FdrDgram(mgr.GetFdrService(), std::move(ses), &mgr);
}
#endif

namespace fon9 {

fon9_API DeviceFactorySP MakeIoFactoryDgram(std::string name) {
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
static bool Dgram_Start(seed::PluginsHolder& holder, StrView args) {
   struct ArgsParser : public DeviceFactoryConfigParser {
      ArgsParser() : DeviceFactoryConfigParser{"Dgram"} {}
      DeviceFactorySP CreateDeviceFactory() override {
         return MakeIoFactoryDgram(this->Name_);
      }
   };
   return ArgsParser{}.Parse(holder, args);
}

} // namespaces

extern "C" fon9_API fon9::seed::PluginsDesc f9p_Dgram;
static fon9::seed::PluginsPark f9pRegister{"Dgram", &f9p_Dgram};

fon9::seed::PluginsDesc f9p_Dgram{
   "",
   &fon9::Dgram_Start,
   nullptr,
   nullptr,
};
