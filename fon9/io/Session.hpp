/// \file fon9/io/Session.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_Session_hpp__
#define __fon9_io_Session_hpp__
#include "fon9/io/IoBase.hpp"
#include "fon9/buffer/DcQueueList.hpp"
#include "fon9/TimeStamp.hpp"

namespace fon9 { namespace io {

enum SendDirectResult {
   SendError,
   Sent,
   Queue,
   NeedsAsync,
   NoLink,
};
class RecvDirectArgs {
   fon9_NON_COPY_NON_MOVE(RecvDirectArgs);
   RecvDirectArgs() = delete;
public:
   DeviceOpLocker&   OpLocker_;
   Device&           Device_;
   DcQueueList&      RecvBuffer_;
   RecvDirectArgs(DeviceOpLocker& opLocker, Device& dev, DcQueueList& rxbuf)
      : OpLocker_(opLocker)
      , Device_(dev)
      , RecvBuffer_(rxbuf) {
   }

   virtual bool IsRecvBufferAlive() const = 0;
   virtual SendDirectResult SendDirect(BufferList&& txbuf) = 0;
};

/// \retval RecvBufferSize::AsyncRecvEvent
///   必須使用 Device Async 操作接收事件.
///   返回後會到 op thread 觸發 OnDevice_Recv() 事件.
/// \retval RecvBufferSize::NoLink
///   發現斷線, 或 e.RecvBuffer_ 已經無效.
/// \retval 其他
///   繼續等候接收事件.
using FnOnDevice_RecvDirect = RecvBufferSize (*)(RecvDirectArgs& e);

fon9_WARN_DISABLE_PADDING;
/// \ingroup io
/// 通訊「處理程序」基底.
/// * 檢核對方的合法性(登入、驗證...)。
/// * 「業務系統」與 `Device` 之間的橋樑。
///   * 負責解析收到的訊息，轉成「業務系統」的業務要求，再將處理結果送回給對方。
///   * 負責訂閱「業務系統」的通知(報告、訊息)，用對方理解的格式送出。
class fon9_API Session : public intrusive_ref_counter<Session> {
   fon9_NON_COPY_NON_MOVE(Session);
public:
   /// 在處理收到訊息時(e.g. DeviceRecvBufferReady()),
   /// 若此處非 nullptr 則叫用此函式處理訊息.
   /// - 若此處為 nullptr 則使用 OnDevice_Recv() 處理收到的訊息.
   /// - 此時的 e.OpLocker_ 尚未建立 ALocker.
   /// - 根據傳回值決定後續動作.
   /// - 收到資料後, 立即觸發此事件, 此時不保證 op queue 為空:
   ///   也就是可能同時 device 正在送出 or 關閉 or 其他操作....
   /// - 此事件適用於:
   ///   - 單向行情接收: 例如接收交易所 multicast 行情.
   ///   - 類似 web server 短連線: 收到 request, 立即處理並回覆結果,
   ///     不會有其他 thread 呼叫 send.
   const FnOnDevice_RecvDirect   FnOnDevice_RecvDirect_;

   Session(FnOnDevice_RecvDirect fnOnDevice_RecvDirect = nullptr) : FnOnDevice_RecvDirect_{fnOnDevice_RecvDirect} {
   }

   virtual ~Session();

   /// Device::Initialized() 初始化通知, 預設: do nothing.
   virtual void OnDevice_Initialized(Device& dev);

   /// Device::~Device() 的通知, 預設: do nothing.
   /// - 收到此事件時, 您必定不會擁有 dev 的 DeviceSP,
   /// - 仍可使用 dev.GetSessionBookmark(); 做一些後續的處理
   /// - 收到此事件後, this session 可能也會被解構(在 dev.Session_ 解構時).
   virtual void OnDevice_Destructing(Device& dev);

   /// 當 Device 狀態改變時通知, 預設: do nothing.
   /// 相同事件不會重複觸發, 也就是 bfst 不可能與 afst 相同.
   /// 除了底下狀態, 其餘都會在 Device 的 thread 裡面通知:
   /// - State::Initializing, State::Initialized
   /// - 不會在此處發 State::Destructing 狀態, 會直接觸發 OnDevice_Destructing() 事件.
   virtual void OnDevice_StateChanged(Device& dev, const StateChangedArgs& e);

   /// 當 Device 狀態沒變, 但更新狀態訊息時通知.
   virtual void OnDevice_StateUpdated(Device& dev, const StateUpdatedArgs& e);

   /// 當 Device 連線成功, 可以收送資料時通知, 預設: do nothing.
   /// - 先觸發 OnDevice_StateChanged(dev, {After_ = State::LinkReady} );
   /// - 然後才是 OnDevice_LinkReady(dev);
   /// - Device 不提供斷線事件, 因為有可能從 LinkReady 直接進入 Closing, Disposing...
   ///   若要判斷斷線, 應該在 OnDevice_StateChanged() 判斷:
   ///   \code
   ///      if (e.Before_ == State::LinkReady) {
   ///         ... 斷線 or 主動關閉 or 重開 or...
   ///      }
   ///   \endcode
   /// - 返回時 **必須決定是否要收資料**, Device 不提供收資料的方法呼叫, 而是在 OnDevice_Recv() 直接提供已收到的資料。
   ///
   /// \retval ==RecvBufferSize::NoRecvEvent 不接收 OnDevice_Recv() 事件.
   /// \retval >=RecvBufferSize::Default     由 Device 自行決定如何處理緩衝區大小, 可在後續的 OnDevice_Recv() 返回時, 調整接收緩衝區大小.
   virtual RecvBufferSize OnDevice_LinkReady(Device& dev);

   /// 當 Device 收到資料時通知, 預設: do nothing & 返回 RecvBufferSize::Default.
   /// 下次收的資料會接續在 rxbuf 之後.
   /// 返回後, 不論 rxbuf 是否仍有剩餘的資料, 都只會在下次有新資料時, 才會再次通知.
   /// 返回時您必須思考, 如何處理緩衝區:
   /// \retval >RecvBufferSize::Default      指定rxbuf需要額外分配的空間, 但下次通知不一定會填滿此空間, 也有可能會超過.
   /// \retval ==RecvBufferSize::Default     由 Device 自行決定如何處理緩衝區大小.
   /// \retval ==RecvBufferSize::NoRecvEvent 不用再收任何資料.
   /// \retval ==RecvBufferSize::CloseRecv   關閉接收端.
   virtual RecvBufferSize OnDevice_Recv(Device& dev, DcQueueList& rxbuf);

   /// 在 LinkReady 之後, 若有啟動 dev CommonTimer: dev.CommonTimerRunAfter(ti),
   /// 則透過此處通知 Session.
   /// 注意: 此時是在 timer thread, 並不是 op safe.
   /// 如果需要 op safe, 則必須自行使用 dev.OpQueue_ 來處理.
   virtual void OnDevice_CommonTimer(Device& dev, TimeStamp now);

   /*
   幾經思考, 似乎已無必要提供此事件:
   因為可加入自訂的 BufferNodeVirtual，在消費到該節點時取得通知，執行必要的後續作業。
   但 `BufferNodeVirtual` 的通知是在「送完 or 傳送發生錯誤」，所以該通知不與 Device 的事件相關，使用時要小心。

   /// 當 Device 的傳送緩衝區資料已送完通知, 預設: do nothing.
   /// - Device 預設關閉此事件, 若有需要此事件, 則透過 Device::EnableSendBufferEmptyEv(); 啟用.
   /// - 這裡是指的是 Device 內部的傳送緩衝區, 不代表系統緩衝區已送完.
   /// - 只有 dev 在 LinkReady 狀態才會觸發此事件.
   virtual void OnDevice_SendBufferEmpty(Device& dev);
   */

   /// 執行特定命令.
   /// - 可在任意 thread 呼叫此處: 衍生者必須特別注意!
   /// - 預設傳回: "unknown session command"
   virtual std::string SessionCommand(Device& dev, StrView cmdln);
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_io_Session_hpp__
