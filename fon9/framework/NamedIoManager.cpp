/// \file fon9/framework/NamedIoManager.cpp
/// \author fonwinz@gmail.com
#include "fon9/framework/IoManagerTree.hpp"
#include "fon9/seed/SysEnv.hpp"
#include "fon9/seed/Plugins.hpp"

namespace fon9 {

static bool NamedIoManager_Start(seed::PluginsHolder& holder, StrView args) {
   IoManagerArgs iomArgs;
   while (!args.empty()) {
      StrView fld = SbrFetchNoTrim(args, '|');
      StrView tag = StrFetchTrim(fld, '=');
      StrTrim(&fld);
      if (tag == "Name")
         iomArgs.Name_ = fld.ToString();
      else if (tag == "DeviceFactoryPark" || tag == "DevFp")
         iomArgs.DeviceFactoryPark_ = seed::FetchNamedPark<DeviceFactoryPark>(*holder.Root_, fld);
      else if (tag == "SessionFactoryPark" || tag == "SesFp")
         iomArgs.SessionFactoryPark_ = seed::FetchNamedPark<SessionFactoryPark>(*holder.Root_, fld);
      else if (tag == "IoService" || tag == "Svc")
         iomArgs.IoServiceSrc_ = holder.Root_->GetSapling<IoManager>(fld);
      else if (tag == "IoServiceCfg" || tag == "SvcCfg") //"ThreadCount=2|Capacity=100"
         iomArgs.IoServiceCfgstr_ = SbrFetchInsideNoTrim(fld).ToString();
      else if (tag == "Config" || tag == "Cfg") {
         std::string  cfgfn = SysEnv_GetConfigPath(*holder.Root_).ToString();
         fld.AppendTo(cfgfn);
         iomArgs.CfgFileName_ = StrView_ToNormalizeStr(&cfgfn);
      }
   }
   if (!iomArgs.Name_.empty() && IoManagerTree::Plant(*holder.Root_, iomArgs))
      return true;
   holder.SetPluginsSt(fon9::LogLevel::Error, "Name=", iomArgs.Name_, "|err='Name' is dup or empty");
   return false;
}

} // namespaces

extern "C" fon9_API fon9::seed::PluginsDesc f9p_NamedIoManager;
static fon9::seed::PluginsPark f9p_NamedIoManager_reg{"NamedIoManager", &f9p_NamedIoManager};

fon9::seed::PluginsDesc f9p_NamedIoManager{
   "",
   &fon9::NamedIoManager_Start,
   nullptr,
   nullptr,
};
