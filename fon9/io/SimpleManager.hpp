/// \file fon9/io/SimpleManager.cpp
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
class SimpleManager : public Manager {
   fon9_NON_COPY_NON_MOVE(SimpleManager);

public:
   SimpleManager() = default;

   virtual void OnDevice_Destructing(Device& dev) override {
      fon9_LOG_INFO("OnDevice_Destructing|dev=", ToPtr{&dev});
   }
   void OnDevice_Accepted(DeviceServer& server, DeviceAcceptedClient& client) {
      fon9_LOG_INFO("OnDevice_Accepted|server=", ToPtr(&server), "|client=", ToPtr(&client));
   }
   virtual void OnDevice_Initialized(Device& dev) override {
      fon9_LOG_INFO("OnDevice_Initialized|dev=", ToPtr{&dev});
   }
   void OnDevice_StateChanged(Device& dev, const StateChangedArgs& e) {
      fon9_LOG_INFO("OnDevice_StateChanged|dev=", ToPtr{&dev},
                    "|st=", GetStateStr(e.After_),
                    "|id={", e.DeviceId_, "}"
                    "|info=", e.Info_);
   }
   void OnDevice_StateUpdated(Device& dev, const StateUpdatedArgs& e) {
      fon9_LOG_INFO("OnDevice_StateUpdated|dev=", ToPtr{&dev},
                    "|st=", GetStateStr(e.State_),
                    "|info=", e.Info_);
   }
   void OnSession_StateUpdated(Device& dev, StrView stmsg) {
      fon9_LOG_INFO("OnSession_StateUpdated|dev=", ToPtr{&dev}, "|stmsg=", stmsg);
   }
};

} } // namespaces
#endif//__fon9_io_SimpleManager_hpp__
