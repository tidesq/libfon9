/// \file fon9/io/SimpleManager.cpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_SimpleManager_hpp__
#define __fon9_io_SimpleManager_hpp__
#include "fon9/io/Manager.hpp"
#include "fon9/Log.hpp"

namespace fon9 { namespace io {

/// \ingroup io
/// 簡單的 io Manager 實作:
/// - 使用 fon9 log 機制記錄 Device 的事件.
class SimpleManager : public Manager {
public:
   virtual void OnDevice_Destructing(Device& dev) override {
      fon9_LOG_INFO("OnDevice_Destructing|dev=", &dev);
   }
   void OnDevice_Accepted(DeviceServer& server, Device& client) {
      fon9_LOG_INFO("OnDevice_Accepted|server=", &server, "|client=", &client);
   }
   virtual void OnDevice_Initialized(Device& dev) override {
      fon9_LOG_INFO("OnDevice_Initialized|dev=", &dev);
   }
   void OnDevice_StateChanged(Device& dev, const StateChangedArgs& e) {
      fon9_LOG_INFO("OnDevice_StateChanged|dev=", &dev,
                    "|id={", e.DeviceId_, "}"
                    "|st=", GetStateStr(e.After_),
                    "|info=", e.Info_);
   }
   void OnDevice_StateUpdated(Device& dev, const StateUpdatedArgs& e) {
      fon9_LOG_INFO("OnDevice_StateUpdated|dev=", &dev,
                    "|st=", GetStateStr(e.State_),
                    "|info=", e.Info_);
   }
   void OnSession_StateUpdated(Device& dev, StrView stmsg) {
      fon9_LOG_INFO("OnSession_StateUpdated|dev=", &dev, "|stmsg=", stmsg);
   }
};

} } // namespaces
#endif//__fon9_io_SimpleManager_hpp__
