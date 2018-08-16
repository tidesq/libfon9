/// \file fon9/io/IoBase.hpp
/// \author fonwinz@gmail.com
/// \defgroup io  IO通訊
#ifndef __fon9_io_IoBase_hpp__
#define __fon9_io_IoBase_hpp__
#include "fon9/StrView.hpp"
#include "fon9/intrusive_ref_counter.hpp"
#include "fon9/Utility.hpp"

#include <memory>

namespace fon9 { namespace io {

class Device;
template <class DeviceT>
using DeviceSPT = intrusive_ptr<DeviceT>;
/// \ingroup io
/// Device 的指標, 一般而言在收到 fon9::io::State::Disposing 事件時, 必須要 reset();
using DeviceSP = DeviceSPT<Device>;
class DeviceServer;
class DeviceAcceptedClient;
struct DeviceOpLocker;

class Session;
template <class SessionT>
using SessionSPT = intrusive_ptr<SessionT>;
using SessionSP = SessionSPT<Session>;

class Manager;
using ManagerSP = intrusive_ptr<Manager>;

struct DeviceFactoryPark;
using DeviceFactoryParkSP = intrusive_ptr<DeviceFactoryPark>;

struct SessionFactoryPark;
using SessionFactoryParkSP = intrusive_ptr<SessionFactoryPark>;

//--------------------------------------------------------------------------//

/// \ingroup io
/// Device 的類型.
enum class Style {
   /// 不認識的類型.
   Unknown,
   /// 虛擬設備, 例如 DeviceFileIO
   Simulation,
   /// 硬體設備, 例如 USB port, RS232...
   Hardware,
   /// 等候連線端, 例如: TcpServer.
   /// 本身無法收送資料, 僅等候 Client 連入之後建立新的 Device.
   Server,
   /// 由 Server 建立的 Device.
   AcceptedClient,
   /// 發起連線端, 例如: TcpClient.
   Client,
   /// 等候連入端, 例如: 等候連入的 Modem 、等候連入的電話設備...
   LinkIn,
};

/// \ingroup io
/// Device 的狀態變化: 這裡的狀態變化順序與 enum 的數值有關, 不可隨意調整順序.
enum class State {
   /// 初始化尚未完成: Device 建構之後的初始狀態.
   Initializing,
   /// 建構 Device 的人必須主動呼叫 Device::Initialize() 才會進入此狀態.
   Initialized,

   /// Device::OpImpl_Open() 因設定錯誤的開啟失敗。
   /// 無法使用相同 cfgstr 再次開啟.
   ConfigError,
   /// 開啟中: 開啟尚未完成.
   /// 若開啟完成, 則進入底下3種可能的狀態, 視不同的 Device Style 而定:
   /// - WaitingLinkIn
   /// - Linking
   /// - Listening
   Opening,

   /// 連入等候中: 例如一個 USB port, 正在等候 USB 設備的插入.
   WaitingLinkIn,
   /// 連線建立中: 例如 TcpClient 呼叫了 connect().
   Linking,
   /// 連線建立失敗: 例如 TcpClient 呼叫 connect() 之後沒有連線成功.
   LinkError,
   /// 連線成功: 可以開始收送資料.
   /// - WaitingLinkIn, Linking 之後, 連線成功的狀態.
   LinkReady,
   /// 連線中斷.
   /// - **LinkReady 之後** , 連線中斷了!
   LinkBroken,

   /// Listening(例如: TcpServer)
   Listening,
   /// Listening之後被中斷: reset 網卡/IP?
   ListenBroken,

   /// 逗留中, 等資料送完後關閉.
   Lingering,
   /// 關閉中.
   Closing,
   /// 已關閉.
   Closed,

   /// 即將解構, Device 已不可再使用!
   /// 當收到此狀態, 您應該盡快將 Device 釋放.
   Disposing,
   /// 在 Device::~Device() 最後的解構狀態.
   Destructing,
};
/// \ingroup io
/// 取得 st 的顯示字串.
fon9_API StrView GetStateStr(State st);

inline bool IsAllowContinueSend(State st) {
   return st == State::LinkReady || st == State::Lingering;
}

//--------------------------------------------------------------------------//

/// \ingroup io
/// OnDevice_StateChanged() 事件的參數.
struct StateChangedArgs {
   fon9_NON_COPY_NON_MOVE(StateChangedArgs);
   StateChangedArgs(StrView info, const std::string& deviceId)
      : Info_{info}
      , DeviceId_(deviceId) {
   }
   State                Before_;
   State                After_;
   /// if (After _== State::Opening) 則 Info_ = cfgstr;
   StrView              Info_;
   const std::string&   DeviceId_;
};

fon9_WARN_DISABLE_PADDING;
/// \ingroup io
/// OnDevice_StateUpdated() 事件的參數.
/// State 沒變, 但更新訊息.
/// 例如: 還在 State::Linking, 但是嘗試不同的 RemoteAddress.
struct StateUpdatedArgs {
   StateUpdatedArgs(State st, StrView info)
      : State_(st)
      , Info_{info} {
   }
   State    State_;
   StrView  Info_;
};
fon9_WARN_POP;

//--------------------------------------------------------------------------//

/// \ingroup io
/// 接收緩衝區大小.
/// 若要自訂大小, 可自行使用 static_cast<RecvBufferSize>(2048);
enum class RecvBufferSize : int32_t {
   /// 不收資料: 不產生 OnDevice_Recv() 事件.
   NoRecvEvent = -1,
   /// 關閉接收端, 在 Socket Device 可能觸發 shutdown(so, SHUT_RD);
   CloseRecv = -2,

   /// FnOnDevice_RecvDirect_() 處理過程發現斷線, 或 rxbuf 已經無效.
   NoLink = -3,
   /// FnOnDevice_RecvDirect_() 處理過程發現必須使用 Device Async 操作接收事件.
   /// 應到 op thread 觸發 OnDevice_Recv() 事件.
   AsyncRecvEvent = -4,

   /// 交給 Device 自行決定接收緩衝區大小.
   Default = 0,
   /// 最少 128 bytes.
   B128 = 128,
   /// 最少 1024 bytes.
   B1024 = 1024,

   B1K = B1024,
   B4K = B1K * 4,
   B8K = B1K * 8,
   B64K = B1K * 64,
};
class RecvBuffer;

//--------------------------------------------------------------------------//

enum class DeviceFlag {
   SendASAP = 0x01,
   /// 若 `RecvBufferSize OnDevice_LinkReady();` 或 `RecvBufferSize OnDevice_Recv();`
   /// 傳回 RecvBufferSize::NoRecvEvent, 則會關閉 OnDevice_Recv() 事件.
   NoRecvEvent = 0x02,
};
fon9_ENABLE_ENUM_BITWISE_OP(DeviceFlag);

fon9_WARN_DISABLE_PADDING;
struct DeviceOptions {
   DeviceFlag  Flags_{DeviceFlag::SendASAP};

   // ms, =0 表示 LinkError 時, 不用 reopen, 預設為 15 秒.
   uint32_t    LinkErrorRetryInterval_{15000};
   // ms, =0 表示 LinkBroken 時, 不用 reopen, 預設為 3 秒.
   uint32_t    LinkBrokenReopenInterval_{3000};

   /// 設定屬性參數:
   /// - SendASAP=N        預設值為 'Y'，只要不是 'N' 就會設定成 Yes(若未設定，初始值為 Yes)。
   /// - RetryInterval=n   LinkError 之後重新嘗試的延遲時間, 預設值為 15 秒, 0=不要 retry.
   /// - ReopenInterval=n  LinkBroken 或 ListenBroken 之後, 重新嘗試的延遲時間, 預設值為 3 秒, 0=不要 reopen.
   ///   使用 TimeInterval 格式設定, 延遲最小單位為 ms, e.g.
   ///   "RetryInterval=3"    表示連線失敗後, 延遲  3 秒後重新連線.
   ///   "ReopenInterval=0.5" 表示斷線後, 延遲  0.5 秒後重新連線.
   std::string ParseOption(StrView tag, StrView value);
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_io_IoBase_hpp__
