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
class fon9_API Manager {
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

private:
   virtual unsigned IoManagerAddRef() = 0;
   virtual unsigned IoManagerRelease() = 0;
   friend unsigned intrusive_ptr_add_ref(const Manager* p) { return const_cast<Manager*>(p)->IoManagerAddRef(); }
   friend unsigned intrusive_ptr_release(const Manager* p) { return const_cast<Manager*>(p)->IoManagerRelease(); }
};

class fon9_API ManagerC : public Manager, public intrusive_ref_counter<ManagerC> {
   fon9_NON_COPY_NON_MOVE(ManagerC);
   using baseCounter = intrusive_ref_counter<ManagerC>;
   unsigned IoManagerAddRef() override;
   unsigned IoManagerRelease() override;
public:
   ManagerC() = default;
};
using ManagerCSP = intrusive_ptr<ManagerC>;
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_io_Manager_hpp__
