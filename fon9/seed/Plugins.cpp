// \file fon9/seed/Plugins.cpp
// \author fonwinz@gmail.com
#include "fon9/seed/Plugins.hpp"

namespace fon9 { namespace seed {

PluginsHolder::~PluginsHolder() {
}
void PluginsHolder::LogPluginsSt(LogLevel level, RevBufferList&& rbuf) {
   std::string stmsg;
   LogArgs     logArgs{level};
   RevPrint(rbuf, this->CurFileName_, '#', this->CurEntryName_, '|');
   AddLogHeader(rbuf, logArgs.UtcTime_, level);
   BufferAppendTo(rbuf.cfront(), stmsg);
   if (fon9_UNLIKELY(level >= fon9::LogLevel_))
      LogWrite(logArgs, rbuf.MoveOut());
   stmsg.pop_back(); // 移除尾端的 '\n';
   this->SetPluginStImpl(std::move(stmsg));
}
void PluginsHolder::StopPlugins() {
   if (this->IsRunning_) {
      this->IsRunning_ = false;
      if (const PluginsDesc* plugins = this->GetPluginsDesc()) {
         if (plugins->FnStop_ && plugins->FnStop_(*this))
            this->Close();
         else
            this->Release();
         goto __CLEAR_PLUGINS;
      }
   }
   this->Close();
__CLEAR_PLUGINS:
   this->CurFileName_.clear();
   this->CurEntryName_.clear();
   this->PluginsBookmark_ = 0;
}
bool PluginsHolder::StartPlugins(StrView args) {
   if (!this->IsRunning_)
      if (const PluginsDesc* plugins = this->GetPluginsDesc()) {
         this->IsRunning_ = plugins->FnStart_(*this, args);
         if (this->IsRunning_)
            return true;
         this->StopPlugins();
      }
   return false;
}
std::string PluginsHolder::LoadPlugins(StrView fileName, StrView entryName) {
   if (ToStrView(this->CurFileName_) == fileName && ToStrView(this->CurEntryName_) == entryName)
      return std::string{};
   this->StopPlugins();
   this->CurFileName_.assign(fileName);
   this->CurEntryName_.assign(entryName);
   if (fileName.empty()) {
      if (entryName.empty())
         return std::string{};
      this->Sym_ = const_cast<PluginsDesc*>(PluginsPark::Register(entryName, nullptr));
   }
   else {
      std::string res = this->Open(fileName.ToString().c_str(), entryName.ToString().c_str());
      if (!res.empty())
         return res;
   }
   if (this->Sym_ == nullptr)
      return "Plugins not found";
   return std::string{};
}
//--------------------------------------------------------------------------//
const PluginsDesc* PluginsPark::Register(StrView name, const PluginsDesc* plugins) {
   return SimpleFactoryRegister(name, plugins);
}

} } // namespaces
