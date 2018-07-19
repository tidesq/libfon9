/// \file fon9/InnSyncer.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_InnSyncer_hpp__
#define __fon9_InnSyncer_hpp__
#include "fon9/SortedVector.hpp"
#include "fon9/CharVector.hpp"
#include "fon9/intrusive_ref_counter.hpp"
#include "fon9/MustLock.hpp"
#include "fon9/buffer/DcQueue.hpp"
#include "fon9/buffer/RevBufferList.hpp"

namespace fon9 {

class fon9_API InnSyncer;

fon9_WARN_DISABLE_PADDING;
/// \ingroup Inn
/// 同步訊息處理者: InnSyncer 收到同步資料後, 交給這裡處理.
class fon9_API InnSyncHandler : public intrusive_ref_counter<InnSyncHandler> {
   fon9_NON_COPY_NON_MOVE(InnSyncHandler);
public:
   /// 每個 InnSyncer 可能同步多個資料集(e.g. InnDbf),
   /// 所以 InnSyncer 透過 SyncHandlerName_ 來決定一筆訊息要交給哪個 Handler 處理.
   const CharVector  SyncHandlerName_;

   /// - syncerName.size() 必須 <= 0xff:
   ///   在 InnSyncer 寫入同步資料時, 僅用 1 byte 儲存 syncerName 的長度.
   InnSyncHandler(const StrView& name) : SyncHandlerName_{name} {
   }
   virtual ~InnSyncHandler();

   /// 收到一筆資料後立即觸發通知, buf 只包含一個封包.
   /// 由 Handler 自行決定如何處理, 每次應完全處理 buf 的內容.
   /// 未處理完的內容將會被拋棄.
   virtual void OnInnSyncReceived(InnSyncer& sender, DcQueue&& buf) = 0;
   /// 當所有可能的同步來源都告一段落, 不會再有舊的同步訊息, 會觸發此事件.
   /// 此事件做完後才會開始新的同步.
   virtual void OnInnSyncFlushed(InnSyncer& sender) = 0;
};
using InnSyncHandlerSP = intrusive_ptr<InnSyncHandler>;
fon9_WARN_POP;

/// \ingroup Inn
/// 如果多個 InnSyncHandler 共用一個 syncer:
/// - 一定要在全部的 InnSyncHandler 都加入後, 才能啟動 syncer.
///   - 否則可能會有資料遺失, 因為: 找不到 InnSyncHandler 而拋棄同步資料.
class fon9_API InnSyncer : public intrusive_ref_counter<InnSyncer> {
   fon9_NON_COPY_NON_MOVE(InnSyncer);
public:
   InnSyncer() = default;
   virtual ~InnSyncer();

   enum class State {
      ErrorCtor = -2,
      Error = -1,
      Pending,
      Ready,
      Running,
      Stopping,
      Stopped,
   };
   State GetState() const {
      return this->State_;
   }

   /// 開始同步匯入.
   virtual State StartSync() = 0;
   /// 停止同步匯入.
   /// - 確定停止後才會返回, 所以禁止在處理同步匯入時(InnSyncHandler::OnInnSync)呼叫, 避免死結.
   virtual void StopSync() = 0;

   /// 關閉同步通知, 通常在程式結束前, StopSync() 之後呼叫, 避免遺失同步資料.
   /// 禁止在處理同步匯入時(InnSyncHandler::OnInnSync)呼叫, 避免死結.
   /// \retval true 確實有移除 InnSyncHandler.
   /// \retval false 找不到要移除的 InnSyncHandler.
   bool DetachHandler(InnSyncHandler& handler);

   /// 連結同步通知, 通常在 StartSync() 之前呼叫, 避免遺失同步資料.
   /// \retval true  連結成功.
   /// \retval false 連結失敗, handler.SyncHandlerName_ 名稱重複.
   bool AttachHandler(InnSyncHandlerSP handler);

   /// 寫入同步資料.
   /// 由衍生者決定: 立刻寫入(傳送), 或非同步寫入(傳送).
   void WriteSync(InnSyncHandler& handler, RevBufferList&& rbuf);

protected:
   /// 此函式由衍生者完成.
   /// rbuf 包含: 資料大小 + SyncHandlerName + 同步內容.
   virtual void WriteSyncImpl(RevBufferList&& rbuf) = 0;

   /// 判斷 buf 的資料量是否包含依筆完整同步訊息.
   /// 如果完整, 則取出 SyncHandlerName 並通知該 Handler 處理.
   /// \return 共處理了幾筆訊息.
   size_t OnInnSyncRecv(DcQueue& buf);
   void OnInnSyncRecvImpl(StrView handlerName, DcQueue&& buf);

   State State_{};
private:
   using HandlerMapImpl = SortedVector<StrView, InnSyncHandlerSP>;
   using HandlerMap = MustLock<HandlerMapImpl>;
   HandlerMap  HandlerMap_;
};
using InnSyncerSP = intrusive_ptr<InnSyncer>;

} // namespaces
#endif//__fon9_InnSyncer_hpp__
