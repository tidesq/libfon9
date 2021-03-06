﻿/// \file fon9/io/SimpleManager.cpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_SimpleManager_hpp__
#define __fon9_io_SimpleManager_hpp__
#include "fon9/io/Device.hpp"
#include "fon9/Log.hpp"

namespace fon9 { namespace io {

/// \ingroup io
/// 簡單的 io Manager 實作, 這裡僅作為範例及簡單的測試之用,
/// 更好的管理員請使用 class IoManager(名稱未定);
/// - 使用 fon9 log 機制記錄 Device 事件.
/// - 當 Device 發生錯誤: 10秒後重新連線。
class SimpleManager : public ManagerC {
   fon9_NON_COPY_NON_MOVE(SimpleManager);

public:
   SimpleManager() = default;

   void OnDevice_Destructing(Device& dev) override {
      fon9_LOG_INFO("OnDevice_Destructing|dev=", ToPtr{&dev});
   }
   void OnDevice_Accepted(DeviceServer& server, DeviceAcceptedClient& client) override {
      fon9_LOG_INFO("OnDevice_Accepted|server=", ToPtr(&server), "|client=", ToPtr(&client));
   }
   void OnDevice_Initialized(Device& dev) override {
      fon9_LOG_INFO("OnDevice_Initialized|dev=", ToPtr{&dev});
   }

   void UpdateDeviceState(StrView fn, Device& dev, const StateUpdatedArgs& e) {
      fon9_LOG_INFO(fn, "|dev=", ToPtr{&dev},
                    "|st=", GetStateStr(e.State_),
                    "|id={", e.DeviceId_, "}"
                    "|info=", e.Info_);
   }
   void OnDevice_StateChanged(Device& dev, const StateChangedArgs& e) override {
      this->UpdateDeviceState("OnDevice_StateChanged", dev, e.After_);
   }
   void OnDevice_StateUpdated(Device& dev, const StateUpdatedArgs& e) override {
      this->UpdateDeviceState("OnDevice_StateUpdated", dev, e);
   }
   void OnSession_StateUpdated(Device& dev, StrView stmsg, LogLevel lv) override {
      fon9_LOG(lv, "OnSession_StateUpdated|dev=", ToPtr{&dev}, "|stmsg=", stmsg);
   }
};

} } // namespaces
#endif//__fon9_io_SimpleManager_hpp__
