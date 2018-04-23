/// \file fon9/io/SimpleManager.cpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_SimpleManager_hpp__
#define __fon9_io_SimpleManager_hpp__
#include "fon9/io/Device.hpp"
#include "fon9/Log.hpp"
#include "fon9/Timer.hpp"

namespace fon9 { namespace io {

/// \ingroup io
/// 簡單的 io Manager 實作, 這裡僅作為範例及簡單的測試之用,
/// 更好的管理員請使用 class IoManager(名稱未定);
/// - 使用 fon9 log 機制記錄 Device 事件.
/// - 當 Device 發生錯誤: 10秒後重新連線。
class SimpleManager : public Manager {
   static constexpr TimeInterval  GetRetryInterval() {
      return TimeInterval_Second(10);
   }

   struct RetryTimer : public TimerEntry {
      const DeviceSP Device_;
      RetryTimer(Device& dev) : TimerEntry{GetDefaultTimerThread()}, Device_{dev.shared_from_this()} {
      }
      virtual void OnTimer(TimeStamp now) override {
         this->Device_->AsyncOpen(std::string());
      }
   };
   void StartRetryTimer(Device& dev) {
      RetryTimer* timer = reinterpret_cast<RetryTimer*>(dev.GetManagerBookmark());
      if (timer == nullptr) {
         timer = new RetryTimer(dev);
         intrusive_ptr_add_ref(timer);
         dev.SetManagerBookmark(reinterpret_cast<Bookmark>(timer));
      }
      timer->RunAfter(GetRetryInterval());
   }
   void DestroyRetryTimer(Device& dev) {
      if (RetryTimer* timer = reinterpret_cast<RetryTimer*>(dev.GetManagerBookmark())) {
         dev.SetManagerBookmark(0);
         timer->StopAndWait();
         intrusive_ptr_release(timer);
      }
   }
public:
   virtual void OnDevice_Destructing(Device& dev) override {
      fon9_LOG_INFO("OnDevice_Destructing|dev=", &dev, "|mgrBookmark=", ToHex{dev.GetManagerBookmark()});
      assert(dev.GetManagerBookmark() == 0);
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
      fon9_WARN_DISABLE_SWITCH;
      switch (e.After_) {
      case State::LinkError:
      case State::LinkBroken:
      case State::ListenBroken:
         this->StartRetryTimer(dev);
         break;
      default:
         this->DestroyRetryTimer(dev);
         break;
      }
      fon9_WARN_POP;
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
