/// \file fon9/framework/IoFactory.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_framework_IoFactory_hpp__
#define __fon9_framework_IoFactory_hpp__
#include "fon9/seed/NamedPark.hpp"
#include "fon9/seed/Plugins.hpp"
#include "fon9/seed/FieldSchCfgStr.hpp"
#include "fon9/io/Server.hpp"
#include "fon9/ConfigUtils.hpp"

namespace fon9 {

class fon9_API IoManager;
using IoManagerSP = intrusive_ptr<IoManager>;

fon9_WARN_DISABLE_PADDING;
struct IoConfigItem {
   EnabledYN         Enabled_{};
   seed::SchCfgStr   SchArgs_;

   CharVector        SessionName_;
   CharVector        SessionArgs_;

   CharVector        DeviceName_;
   CharVector        DeviceArgs_;

   bool operator==(const IoConfigItem& rhs) const {
      return this->Enabled_ == rhs.Enabled_
         && this->SchArgs_ == rhs.SchArgs_
         && this->SessionName_ == rhs.SessionName_
         && this->SessionArgs_ == rhs.SessionArgs_
         && this->DeviceName_ == rhs.DeviceName_
         && this->DeviceArgs_ == rhs.DeviceArgs_;
   }
};
fon9_WARN_POP;

struct fon9_API IoFactoryConfigParser {
   std::string Name_;
   std::string ErrMsg_;
   IoFactoryConfigParser(std::string defaultName = std::string{}) : Name_{std::move(defaultName)} {
   }
   virtual ~IoFactoryConfigParser();

   bool Parse(seed::PluginsHolder& holder, StrView args);
   virtual bool OnIoManager(seed::PluginsHolder& holder, IoManager& iomgr) = 0;
   virtual bool OnUnknownTag(seed::PluginsHolder& holder, StrView tag, StrView value) = 0;

   void OnFail_AddFactory(StrView tag, StrView value);
   void OnFail_NotFoundFactoryPark(StrView tag, StrView value);
};

//--------------------------------------------------------------------------//

class fon9_API SessionFactory : public seed::NamedSeed {
   fon9_NON_COPY_NON_MOVE(SessionFactory);
   using base = seed::NamedSeed;
public:
   using base::base;
   SessionFactory(Named&& name) : base{std::move(name)} {
   }

   virtual io::SessionSP CreateSession(IoManager& mgr, const IoConfigItem& cfg, std::string& errReason) = 0;
   virtual io::SessionServerSP CreateSessionServer(IoManager& mgr, const IoConfigItem& cfg, std::string& errReason) = 0;
};
using SessionFactoryPark = seed::NamedPark<SessionFactory>;
using SessionFactoryParkSP = intrusive_ptr<SessionFactoryPark>;
using SessionFactorySP = SessionFactoryPark::ObjectSP;

struct fon9_API SessionFactoryConfigParser : public IoFactoryConfigParser {
   using IoFactoryConfigParser::IoFactoryConfigParser;
   bool OnIoManager(seed::PluginsHolder& holder, IoManager& iomgr) override;
   bool OnUnknownTag(seed::PluginsHolder& holder, StrView tag, StrView value) override;
   virtual SessionFactorySP CreateSessionFactory() = 0;
};

//--------------------------------------------------------------------------//

class fon9_API DeviceFactory : public seed::NamedSeed {
   fon9_NON_COPY_NON_MOVE(DeviceFactory);
   using base = seed::NamedSeed;
public:
   using base::base;

   virtual io::DeviceSP CreateDevice(IoManagerSP mgr, SessionFactory& sesFactory, const IoConfigItem& cfg,
                                     std::string& errReason) = 0;
};
using DeviceFactoryPark = seed::NamedPark<DeviceFactory>;
using DeviceFactoryParkSP = intrusive_ptr<DeviceFactoryPark>;
using DeviceFactorySP = DeviceFactoryPark::ObjectSP;

struct fon9_API DeviceFactoryConfigParser : public IoFactoryConfigParser {
   using IoFactoryConfigParser::IoFactoryConfigParser;
   bool OnIoManager(seed::PluginsHolder& holder, IoManager& iomgr) override;
   bool OnUnknownTag(seed::PluginsHolder& holder, StrView tag, StrView value) override;
   virtual DeviceFactorySP CreateDeviceFactory() = 0;
};

//--------------------------------------------------------------------------//

fon9_API DeviceFactorySP MakeIoFactoryTcpClient(std::string name = "TcpClient");
fon9_API DeviceFactorySP MakeIoFactoryTcpServer(std::string name = "TcpServer");

} // namespaces
#endif//__fon9_framework_IoFactory_hpp__
