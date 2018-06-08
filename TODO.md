libfon9 TODO list
=======================

---------------------------------------
## 演算法/容器
* LevelArray
---------------------------------------
## 雜項
* SchTask
---------------------------------------
## 檔案/儲存/載入
* Serialize/Deserialize
* InnFile
* 資料同步機制
---------------------------------------
## 系統管理工具
* 盡量從 [非侵入式] 方向思考:
  * 例如: 已有一個 `商品資料表`，則現有程式碼不變，
    只要設計一個: 連接 `root` 與 `商品資料表` 的 `entry`，就可以透過該 entry 管理 `商品資料表`。
* 系統管理基礎建設 (Seed 機制)
* User Auth 使用者管理機制
  * SASL (SCRAM-SHA-1)
---------------------------------------
## 通訊基礎建設
* TLS
* UDP/Multicast
* FileDevice
* IoManager
* SessionFactory/DeviceFactory/FactoryPark
---------------------------------------
## Simple FIX engine
