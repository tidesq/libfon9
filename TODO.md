libfon9 TODO list
=======================

---------------------------------------
## 演算法/容器
---------------------------------------
## 雜項
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
  * 啟動時設定工作目錄 current dir: `-w dir` or `--workdir dir`.
  * 將啟動時的「命令列參數」及「其他重要資訊」寫入 log.
  * Linux: to daemon.
  * Windows: install to service.

---------------------------------------
## 通訊基礎建設
* IoManager/SessionFactory/DeviceFactory/FactoryPark
* UDP/Multicast
* FileDevice
* TLS
---------------------------------------
## Simple FIX engine
