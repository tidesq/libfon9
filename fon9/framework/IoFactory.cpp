/// \file fon9/framework/IoFactory.cpp
/// \author fonwinz@gmail.com
#include "fon9/framework/IoFactory.hpp"
#include "fon9/framework/IoManager.hpp"

namespace fon9 {

IoFactoryConfigParser::~IoFactoryConfigParser() {
}
void IoFactoryConfigParser::OnFail_NotFoundFactoryPark(StrView tag, StrView value) {
   this->ErrMsg_ += "|err=Not found factory park: '" + tag.ToString() + "=" + value.ToString() + "'";
}
void IoFactoryConfigParser::OnFail_AddFactory(StrView tag, StrView value) {
   this->ErrMsg_ += "|err=Add factory fail: 'Name=" + this->Name_
                  + "|" + tag.ToString() + "=" + value.ToString() + "'";
}
bool IoFactoryConfigParser::Parse(seed::PluginsHolder& holder, StrView args) {
   while (!args.empty()) {
      StrView value = SbrFetchNoTrim(args, '|');
      StrView tag = StrFetchTrim(value, '=');
      StrTrim(&value);
      if (tag == "Name")
         this->Name_ = value.ToString();
      else if (tag == "IoManager" || tag == "IoMgr") {
         if (auto iomgr = holder.Root_->GetSapling<IoManager>(value)) {
            if (!this->OnIoManager(holder, *iomgr))
               this->OnFail_AddFactory(tag, value);
         }
         else
            this->OnFail_NotFoundFactoryPark(tag, value);
      }
      else if(!this->OnUnknownTag(holder, tag, value))
         this->ErrMsg_ += "|err=Unknown tag: '" + tag.ToString() + "=" + value.ToString() + "'";
   }
   if (!this->ErrMsg_.empty())
      holder.SetPluginsSt(LogLevel::Error, this->ErrMsg_);
   return true;
}
//--------------------------------------------------------------------------//
bool SessionFactoryConfigParser::OnIoManager(seed::PluginsHolder&, IoManager& iomgr) {
   if (auto fac = this->CreateSessionFactory())
      return iomgr.SessionFactoryPark_->Add(std::move(fac));
   return false;
}
bool SessionFactoryConfigParser::OnUnknownTag(seed::PluginsHolder& holder, StrView tag, StrView value) {
   if (tag == "SessionFactoryPark" || tag == "AddTo") {
      if (auto sesfp = seed::FetchNamedPark<SessionFactoryPark>(*holder.Root_, value)) {
         if (auto fac = this->CreateSessionFactory()) {
            if (!sesfp->Add(std::move(fac)))
               this->OnFail_AddFactory(tag, value);
         }
      }
      else
         this->OnFail_NotFoundFactoryPark(tag, value);
      return true;
   }
   return false;
}
//--------------------------------------------------------------------------//
bool DeviceFactoryConfigParser::OnIoManager(seed::PluginsHolder&, IoManager& iomgr) {
   if (auto fac = this->CreateDeviceFactory())
      return iomgr.DeviceFactoryPark_->Add(std::move(fac));
   return false;
}
bool DeviceFactoryConfigParser::OnUnknownTag(seed::PluginsHolder& holder, StrView tag, StrView value) {
   if (tag == "DeviceFactoryPark" || tag == "AddTo") {
      if (auto devfp = seed::FetchNamedPark<DeviceFactoryPark>(*holder.Root_, value)) {
         if (auto fac = this->CreateDeviceFactory()) {
            if (!devfp->Add(std::move(fac)))
               this->OnFail_AddFactory(tag, value);
         }
      }
      else
         this->OnFail_NotFoundFactoryPark(tag, value);
      return true;
   }
   return false;
}

} // namespaces
