/// \file fon9/io/Manager.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_Manager_hpp__
#define __fon9_io_Manager_hpp__
#include "fon9/io/IoBase.hpp"
#include "fon9/Log.hpp"

namespace fon9 { namespace io {

fon9_WARN_DISABLE_PADDING;
/// \ingroup io
/// Device 管理員基底.
///
/// 簡單的功能實作可參考 `fon9/io/SimpleManager.hpp`
/// 更進階的功能交給「fon9 系統管理工具」完成後再說:
/// - 設定管理: 包含設定的「儲存及載入」.
/// - 排程管理: 何時「啟用 / 結束」某條設定.
/// - 與系統管理員互動.
/// - 連線失敗、斷線: 定時重新開啟.
///
class fon9_API Manager : public intrusive_ref_counter<Manager> {
   fon9_NON_COPY_NON_MOVE(Manager);
public:
   Manager() = default;
   virtual ~Manager();

   /// 當有 client 連線到 server 時的通知, 事件順序:
   /// - OnDevice_Accepted(server, client);
   /// - OnDevice_Initialized(client);
   virtual void OnDevice_Accepted(DeviceServer& server, DeviceAcceptedClient& client) = 0;

   virtual void OnDevice_Initialized(Device& dev) = 0;
   virtual void OnDevice_Destructing(Device& dev) = 0;
   virtual void OnDevice_StateChanged(Device& dev, const StateChangedArgs& e) = 0;
   virtual void OnDevice_StateUpdated(Device& dev, const StateUpdatedArgs& e) = 0;

   virtual void OnSession_StateUpdated(Device& dev, StrView stmsg, LogLevel lv) = 0;
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_io_Manager_hpp__
