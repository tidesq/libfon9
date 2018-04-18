libfon9 基礎建設程式庫 - I/O 通訊
=================================

## 基本說明
* 從「業務系統」角度思考
  * 其實不應該太在乎客戶的要求是從那兒來的。
    * 一定是 TCP 嗎? 有透過 TLS 嗎? 不能用 UDP 嗎? 可以用語音電話嗎?
    * 由於選擇太多，所以 fon9 把與外界溝通的工作抽出來: `class io::Device`
  * 也不用理會與客戶端的通訊協定。
    * FIX Protocol? 自訂協定?
    * 所以 fon9 把協定處理的工作抽出來: `class io::Session`
  * 為了讓「業務系統」能更獨立於通訊及協定之外。
    * 我們增加了一個 `class io::Manager` 來管理 `Device` 及 `Session`
    * 這樣一來「業務系統」只要設計好相關介面，
      就不用理會實際來源，也不用考慮 `Device`、`Session` 的操作、生死之類的問題了。
```
                     +---------+
                     | Manager |
                     +---------+
                    /           \
              +--------+      +---------+
customer <--> | Device | <--> | Session | <--> business system
              +--------+      +---------+
```
---------------------------------------

## `io::Device`
* 負責與「實際的通訊設備」溝通、收送訊息(byte stream)。
* 操作 & 傳送:
  * 通常 device 的操作(Open、Close...)不是 thread safe、且有順序性，所以要將 device 的操作集中到「操作 thread」依序處理。
    因此每個 device 都有一個自己的 task queue: `DeviceOpQueue`，
    預設 `Device::MakeCallForWork()` 透過 `fon9::GetDefaultThreadPool()` 執行 `DeviceOpQueue`。
  * 可參考 [`fon9/AQueue.hpp`](../AQueue.hpp)、[`fon9/io/DeviceAsyncOp.hpp`](DeviceAsyncOp.hpp)
  * 因為「device 操作」不需要考慮「低延遲」，所以可以丟到「操作 thread」去等候排隊。
    但 `SendASAP()` 則有「低延遲」的要求，所以要考慮「當下」是否允許立即送出。
  * 送訊息緩衝處理 [`fon9/io/SendBuffer.hpp`](SendBuffer.hpp)
    * `io::SendBuffer` 不使用 `DeviceOpQueue`，所以 `SendASAP()` 不理會當下是否有「尚未處理」或「正在處理」的操作。
      但是如果有其他 thread「正在傳送」或「已有傳送訊息在排隊」，則當下的傳送要求，會自動變成 `SendBuffered()`。
    * `SendASAP()`: 如果當下情況允許「送訊息」應該要立即或盡快送出，不必回到「傳送 thread」去送(e.g. event loop thread)。
    * `Send()` 可經由設定決定使用 `SendASAP()` or `SendBuffered()`，或可由開發者直接呼叫 `SendASAP()` or `SendBuffered()`。
    * `SendASAP()` vs `SendBuffered()`
      * 在 **瞬間多筆** (瞬間 Send 多次) 的情況下:
        * `SendASAP()` 可能會有較大的「呼叫延遲」，但「資料延遲」可能較低。
          * 因為每次呼叫都會「要求系統立即」送出。
        * `SendBuffered()` 可能會有較低的「呼叫延遲」，但「資料延遲」可能較高。
          * 可是如果資料累積到某數量，因 `SendBuffered()` 會「整批送出」，所以「較晚的資料」延遲可能反而較低。
      * 在 **冷系統** (偶而 Send 一次) 的情況下:
        * 因 `SendBuffered()` 還有個「喚醒傳送」的工作(可能會有系統呼叫、或引發 thread switch)，所以「呼叫延遲」不一定會較低。
      * 要用哪種方式傳送，需視實際應用而定。
* 通知 & 接收:
  * device 的事件通知 (包含: 連線成功通知、狀態改變通知、資料到達通知...)，必須有順序性。
    * 不可多重通知，不應出現: Thr-A 正在通知「資料到達」在尚未結束前，Thr-B 卻同時觸發了「狀態改變」。
    * 當現在沒有事件通知時，若有「資料到達」應在當下立即通知， **不應切換** 到某個「事件通知 thread」。
    * 是否需要「傳送緩衝已送完」通知?
      * 因為可加入自訂的 `BufferNodeVirtual`，在消費到該節點時取得通知，執行必要的後續作業。
      * 因此「傳送緩衝已送完」的通知似乎沒有必要存在了?
  * 收訊息緩衝處理 `io::RecvBuffer`
    * 消費端(Session)可直接使用或取出需要的訊息量，剩餘的在下次有新資料時繼續處理。

## `io::Session`
* 檢核對方的合法性(登入、驗證...)。
* 「業務系統」與 `Device` 之間的橋樑。
  * 負責解析收到的訊息，轉成「業務系統」的業務要求，再將處理結果送回給對方。
  * 負責訂閱「業務系統」的通知(e.g. 成交回報、委託回報、開收盤通知...)，用對方理解的格式送出。

## `io::Manager`
* 執行中的 `Device`、`Session` 管理。
  * 管理 `Device`、`Session` 的死亡。
  * 動態加入 Device (e.g. TCP Server: Accepted client)
* 設定
  * Sch 排程: 啟用時間設定
  * DevName + DevConfig
  * SesName + SesConfig
  * 設定的儲存及載入

## `io::DeviceFactory`
* tcp
* tcps
* file

## `io::SessionFactory`
