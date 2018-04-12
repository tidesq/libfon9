libfon9 基礎建設程式庫 - Filing
===============================

## File/Path tools
* [`fon9/File.hpp`](../fon9/File.hpp)
  * 在讀寫時指定位置(由 OS 保證)，避免不同 thread 同時讀寫，造成 seek + rw 之間交錯造成的問題。
  * Append 開檔模式，由 OS 保證寫入時，一定寫到檔案尾端。
* [`fon9/FilePath.hpp`](../fon9/FilePath.hpp)
  * 檔名拆解、組合工具。
  * 建立路徑工具。
* [`fon9/TimedFileName.hpp`](../fon9/TimedFileName.hpp)
  * 由 [時間] 及 [檔案大小(超過指定大小則增加檔名序號)] 決定檔名。

## Serialize/Deserialize

## InnFile

## 資料同步機制
