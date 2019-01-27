libfon9 TODO list
=======================

---------------------------------------
## 演算法/容器
---------------------------------------
## 雜項
* DefaultThreadPoolArgs: ThreadCount_, CpuAffinity_;
* SerializeNamed() 改用 RevPrint().
  * 及 AppendFieldConfig(); AppendFieldsConfig(); 也一起改.

---------------------------------------
## 檔案/儲存/載入
* Serialize/Deserialize
  * Text
* 資料同步機制: 清除「Deleted 紀錄」的時機。
  * InnSyncFileList: 同步匯入檔, 可能包含來源 HostId, 定時掃描全部的來源檔.
---------------------------------------
## Financial market
* SymbolId/SymbolTable/SymbolTree
  * fon9::Trie / std::unordered / std::map
  * SymbolId: Id, SettleYYYYMM, StrikePrice...
  * Symbol(Multi legs?)
* Simple FIX engine 文件.
---------------------------------------
## 系統管理工具
* web.seed: Search key.


* 設定檔備份機制:
  * 載入成功後備份?
  * 異動寫入前備份?
* AclPolicy: NeedsApply


* WebUI
  * hide column(field)
  * 根據欄位 typeid 決定 cell text align.
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
      * Enter: Start edit, or [Confirm edit], or [@KeyColumn: Move to sapling].
      * F2:    Start edit
      * Space: @KeyColumn: Move to sapling.
      * Backspace: 回到上一層.
      * [Ins]:       Add row
      * [Ctrl-Ins]:  Copy row
      * [Shift-Ins]: Paste row
      * [Ctrl-Del]:  Delete row
      * [Shift-Del]: Cut row
      * ????:  Move to prev tree. (使用 Browser History API)
      * [Alt +]: Add an unordered row for watching. or reload a row.
      * [Alt -]: Remove a watching unordered row.
    * 滑鼠操作:
      * double click: Start edit, or [@KeyColumn: Move to sapling].
      * 路徑列: 切換路徑 or 輸入路徑
* Fon9Co
  * Linux: to daemon.
  * Windows: install to service.
  * 啟動時參數指定預設的 LogLevel.

---------------------------------------
## 通訊基礎建設
* IoManager/SessionFactory/DeviceFactory
  * 提供 IoManager 的使用說明.
  * ip白名單、黑名單?
* Http
  * 限制訊息最大長度.
  * Http SASL: [標準還在草稿階段](https://tools.ietf.org/id/draft-vanrein-httpauth-sasl-00.html)
    看起來還有很遠的路要走，所以只好先用 js 頂著。
  * HttpMessage 提供可將 chunk data 移除的功能, 避免 chunk data 一直長大.
* TLS
  * https://github.com/facebookincubator/fizz
  * Device.DeviceCommand() 傳回值: success+message or fail+message.
