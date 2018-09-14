libfon9 TODO list
=======================

---------------------------------------
## 演算法/容器
---------------------------------------
## 雜項
* DefaultThreadPoolArgs: ThreadCount_, CpuAffinity_;

---------------------------------------
## 檔案/儲存/載入
* Serialize/Deserialize
  * Text
* 資料同步機制: 清除「Deleted 紀錄」的時機。
  * InnSyncFileList: 同步匯入檔, 可能包含來源 HostId, 定時掃描全部的來源檔.
---------------------------------------
## 系統管理工具
* 盡量從 [非侵入式] 方向思考:
  * 例如: 已有一個 `商品資料表`，則現有程式碼不變，
    只要設計一個: 連接 `root` 與 `商品資料表` 的 `entry`，就可以透過該 entry 管理 `商品資料表`。
* WebUI
  * Seed command: command list(help info), contextmenu...
  * hide column(field)
  * 根據欄位 typeid 決定 cell text align.
  * NeedsApply:
    * 使用「套用」方式處理資料異動。
    * 主管確認機制。
  * 資料量太大時的顯示方式(分頁? 捲動時自動載入?)
  * Column:
    * resize:
      * maybe use <colgroup> & <col> & {resize:horizontal}
      * and save to window.localStorage.
    * {text-overflow: ellipsis}
  * Browser History
  * fon9seed.html 操作說明頁:
    * 鍵盤操作: <kbd>
      * ← ↑ → ↓ Move cell focus
      * Home, PgUp, PgDn, End.
      * ESC:   Cancel edit
      * Enter: Start edit, or [Confirm], or [@KeyColumn: Move to sapling].
      * F2:    Start edit
      * Space: @KeyColumn: Move to sapling.
      * Backspace: 回到上一層.
      * [Ins]:       Add row
      * [Ctrl-Ins]:  Copy row
      * [Shift-Ins]: Paste row
      * [Ctrl-Del]:  Delete row
      * [Shift-Del]: Cut row
      * ????:  Move to prev tree. (使用 Browser History API)
    * 滑鼠操作:
      * double click: Start edit, or [@KeyColumn: Move to sapling].
      * 路徑列: 切換路徑 or 輸入路徑
* DllManager
* Fon9Co
  * Linux: to daemon.
  * Windows: install to service.
  * 啟動時參數指定預設的 LogLevel.

---------------------------------------
## 通訊基礎建設
* IoManager/SessionFactory/DeviceFactory
  * 提供 IoManager 的使用說明.
  * Sch 設定排程時間.
  * 註冊 Factory 異動事件, 讓稍晚註冊的 Factory 可以建立 Device.
  * NeedsApply
  * ip白名單、黑名單?
* Http
  * 限制訊息最大長度.
  * 如何找到要提供給 HttpHandler,HttpSession 的參數?
  * Http SASL: [標準還在草稿階段](https://tools.ietf.org/id/draft-vanrein-httpauth-sasl-00.html)
    看起來還有很遠的路要走，所以只好先用 js 頂著。
  * HttpMessage 提供可將 chunk data 移除的功能, 避免 chunk data 一直長大.
* TLS
  * https://github.com/facebookincubator/fizz
  
* UDP/Multicast
* FileDevice
* Device.DeviceCommand() 傳回值: success+message or fail+message.
* DeviceId 拿掉開頭的 '|' 字元.
---------------------------------------
## Simple FIX engine
