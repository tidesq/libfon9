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
* User Auth 使用者管理機制
  * 使用 seed 機制管理資料表: user、role、policy... 
  * 使用 InnDbf 儲存資料表
  * 使用 SASL 機制處理登入要求: 提供 SCRAM-SHA-256
  * 提供 ACL(Access Control List) policy
* TreeFlag::NeedsApply: 使用「套用」方式處理資料異動。
* DllManager
* WebUI
---------------------------------------
## 通訊基礎建設
* TLS
* UDP/Multicast
* FileDevice
* IoManager
* SessionFactory/DeviceFactory/FactoryPark
---------------------------------------
## Simple FIX engine
