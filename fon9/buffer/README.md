libfon9 / buffer
=======================

```
             +--------+     +---------+
producer --> | buffer | --> | DcQueue | --> consumer
             +--------+     +---------+
```
* producer: input device、FIX Message builder...
* consumer: output device、FIX Message parser...

---------------------------------------

## buffer：用來建立(產生)資料
* 例如：建立一筆要寫入檔案的 Log Message。
* 例如：建立一筆要送給遠端的 FIX Message。
* 例如：從 input device 讀入訊息。

### 根據資料的填入方向分成 2 類：
#### 由後往前填入：[buffer/RevBuffer.hpp](RevBuffer.hpp)
* 大部分的情況下，資料從後往前填，才是建立輸出資料最有效率的方式。
* `RevPrint(rbuf, value, ...);` or `RevPrint(rbuf, value, fmt, ...);`
  * [buffer/RevPrint.hpp](RevPrint.hpp)
* `RevFormat(rbuf, format, ...);`
  * [buffer/RevFormat.hpp](RevFormat.hpp)

#### 由前往後填入：[buffer/FwdBuffer.hpp](FwdBuffer.hpp)
* 在某些無法立即取得全部資料，但必須依序填入的情況，例：
  * 一次收到 n 筆下單要求，必須依序處理下單要求，然後一次ACK處理結果。
  * 接收資料時的緩衝。

### 根據記憶體的使用方式分類：
#### 使用單一記憶體空間
* 使用固定大小的記憶體：當空間不足時 throw exception。
* 使用 malloc/realloc：當空間不足時重新分配，複製舊資料到新空間。
  * fon9 不提供此類方法。

#### 使用記憶體串列
* 這是 fon9 的主要作法。
* 當空間不足時：分配新的 BufferNode，串到原本的 BufferList。
* 可插入控制節點，得知資料的使用狀況。

---------------------------------------

## DcQueue：用來消費資料
* 資料不一定連續
* 可同時適用於 Reactor(Linux)、Proactor(Windows)

### enqueue:
* 複製資料
* 移入一串資料區塊: 使用 BufferList

### consumer: output device：非同步使用，每次使用 n 個區塊：
* `PeekBlock()`：一次取出 n 個區塊的位置及大小。
  * Linux 可用 `writev()`，Windows 可用 `WSASend()`，一次消費多個區塊。
* `PopConsumed()`：移除用掉的資料。
* `ConsumedErr()`：消費失敗，移除全部資料。例如：寫檔失敗、傳送失敗。

### consumer: parser：解析資料內容，轉成內部使用的物件：
* `Peek1()`：取得資料開頭的第1個byte位置。
* `Peek()`：取得資料的內容，必要時複製，但不移除。
* `Read()`：取出資料(並移除)。
