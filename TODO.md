libfon9 TODO list
=======================

---------------------------------------
## 演算法/容器
---------------------------------------
## 雜項
* SchTask
* ConfigLoader
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
* TreeFlag::NeedsApply: 使用「套用」方式處理資料異動。
* DllManager
* WebUI
* Fon9Co
  * Linux: to daemon.
  * Windows: install to service.
  * 啟動參數:
    * SysEnv tree: 揭示啟動時的各項參數.
    * 設定 log path.
    * 設定 config path.
    * 載入基本設定:
      * HostId
      * MaAuth path.
      * Sync path.
      * MemLock.

---------------------------------------
## 通訊基礎建設
* IoManager/SessionFactory/DeviceFactory/FactoryPark
* UDP/Multicast
* FileDevice
* TLS
---------------------------------------
## Simple FIX engine
