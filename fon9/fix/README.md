fon9 FIX engine
===============

## 基本說明
* 僅支援 FIX 4.2 及 4.4

## 基礎元件
* 提供 FIX Message 的解析: `fon9::fix::FixParser`
* FIX Message 打包: `fon9::fix::FixBuilder`
  * 將欄位直接填入輸出緩衝區, 不提供將「欄位列表」打包成 FIX Message 的功能。
  * `fon9::fix::FixBuilder` 填妥大部分「交易內容」相關的欄位,
    其他與 FIX 協定相關的欄位(例如: CompID、BeginString), 則留給 `fon9::fix::FixOutput` 處理。
* 提供 Session 收送訊息記錄、狀態記錄: `fon9::fix::FixRecorder`
* 取回之前送過的資料: `fon9::fix::FixRecorder::ReloadSent` 
* `fon9::fix::FixOutput`
  * 完成完整的 FIX 訊息:
    * 填入 CompIDs
    * 填入 SendingTime
    * 填入 MsgSeqNum
    * 填入 MsgType
    * 填入 BodyLength、BeginString
    * 填入 CheckSum
  * 提供 SequenceReset
  * 提供 Replay
* 訊息接收流程, Logon 必須是第一個訊息:
  * Acceptor
    * Device.LinkReady
    * Recv Logon
    * 檢查認證訊息
    * 用 Logon 提供的資訊取得 FixRecorder
    * 開始 FIX 訊息交換
  * Initiator
    * Device.LinkReady
    * 取得 FixRecorder
    * Send Logon
    * Recv Logon
    * 開始 FIX 訊息交換
  * 由於 Acceptor 必須在確認 Logon 之後才能取得正確的 FixRecorder,
    所以 Logon 訊息必須由 FixSession 自行處理.
  * 在進入 FIX 訊息交換階段, 才將收到的訊息(包含 Logon)
    * 交給 `fon9::fix::FixInput` 去確保序號的連續.
    * 確定連續的訊息, 再透過 FixMsgTypeConfig.FixMsgHandler_ 接力處理.
    
