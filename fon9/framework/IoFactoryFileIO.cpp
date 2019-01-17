/// \file fon9/framework/IoFactoryFileIO.cpp
///
/// - 用檔案模擬 Device 輸入/輸出.
///
/// - FileIO factory plugin:
///   - EntryName: FileIO
///   - 參數設定 Args: "Name=FileIO|AddTo=..."
///     - Name=  若沒提供 Name 則預設為 "FileIO"
///     - AddTo= 或 DeviceFactoryPark= 加入到哪個 device factory park.
///     - IoMgr= 或 IoManager= 加入到哪個 IoManager 所參考的 device factory park.
///
/// - FileIO device:
///   - 參數設定 DeviceArgs: 請參考 "fon9/io/FileIO.hpp"
///
/// \author fonwinz@gmail.com
#include "fon9/framework/IoManager.hpp"
#include "fon9/io/FileIO.hpp"

namespace fon9 {

static DeviceFactorySP MakeIoFactoryFileIO(std::string name) {
   struct Factory : public DeviceFactory {
      fon9_NON_COPY_NON_MOVE(Factory);
      Factory(std::string name) : DeviceFactory(std::move(name)) {
      }
      io::DeviceSP CreateDevice(IoManagerSP mgr, SessionFactory& sesFactory, const IoConfigItem& cfg, std::string& errReason) override {
         if (auto ses = sesFactory.CreateSession(*mgr, cfg, errReason))
            return new fon9::io::FileIO(std::move(ses), std::move(mgr));
         return io::DeviceSP{};
      }
   };
   return new Factory(name);
}
static bool DevFileIO_Start(seed::PluginsHolder& holder, StrView args) {
   struct ArgsParser : public DeviceFactoryConfigParser {
      ArgsParser() : DeviceFactoryConfigParser{"FileIO"} {}
      DeviceFactorySP CreateDeviceFactory() override {
         return MakeIoFactoryFileIO(this->Name_);
      }
   };
   return ArgsParser{}.Parse(holder, args);
}

} // namespaces

extern "C" fon9_API fon9::seed::PluginsDesc f9p_FileIO;
static fon9::seed::PluginsPark f9pRegister{"FileIO", &f9p_FileIO};
fon9::seed::PluginsDesc f9p_FileIO{"", &fon9::DevFileIO_Start, nullptr, nullptr,};
