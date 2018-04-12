libfon9 基礎建設程式庫 - Logging
================================

## 基本說明
* [`fon9/Log.hpp`](../fon9/Log.hpp)
* [效率測試及比較](../ext/logvs)
* 基本假設
  * 實際運作時 **不會有瞬間大量** 的 log 需要記錄，所以效率測試增加了「冷系統測試」。
  * log 訊息 **不需要立即寫入** 檔案。寫入時機，例如：每秒寫一次、累積訊息量超過特定數量時。
  * 不同 thread 的訊息，需要依照呼叫順序記錄，不可用 thread local 機制保留，再整批寫入。
  * log 訊息僅需要寫入檔案，可自動依時間(日期)、檔案大小換檔。
    * **不需要清理** 過期的檔案。
    * 若有輸出到其他目的地的需求，則額外再寫一個獨立程式，解析 log 內容，然後轉輸出。
  * 在上述情況下，必須有低延遲的表現。
* 目前看過的 logging library 為了提升效率，用了許多特殊技巧，還是有很多可以借鏡學習的
  * 使用 lock-free queue (bounded queue 、 node based mpsc queue...)
  * 要求 log 記錄時，先不轉字串，僅儲存原始資料(e.g. int、double...)，等到要輸出 log 訊息時再轉成字串。
  * 先暫存於 thread local buffer 等 buffer 滿了再輸出。
* 也有許多 logging library 非常有彈性
  * 可以輸出到不同目的地，例如: system log
  * 提供清理過期檔案功能。
  * 但對我而言，這些屬於錦上添花的特性，反而讓系統更複雜、更難掌控。

## 使用說明
### compile time
* 可以在 compile 主程式時加上底下的定義，關閉特定的 log 訊息:
``` C++
#define fon9_NOLOG_TRACE   // 完全關閉 fon9_LOG_TRACE()
#define fon9_NOLOG_DEBUG   // 完全關閉 fon9_LOG_DEBUG()
#define fon9_NOLOG_INFO    // 完全關閉 fon9_LOG_INFO()
```

### run time
* 可以在執行階段設定要記錄的 log 等級: e.g. `fon9::LogLevel_ = fon9::LogLevel::Error;`

### 初始化
* 如果沒有使用 `InitLogWriteToFile()` 初始化輸出到檔案，則預設使用 stdout 輸出。
* 參數用法請參考 [`fon9/LogFile.hpp`](../fon9/LogFile.hpp)
```c++
File::Result InitLogWriteToFile(std::string           fmtFileName,
                                FileRotate::TimeScale tmScale,
                                File::SizeType        maxFileSize,
                                size_t                highWaterLevelNodeCount);
```

### 記錄 log 訊息
* `fon9_LOG_TRACE(...)`、`fon9_LOG_DEBUG(...)`、`fon9_LOG_INFO(...)`...
* 其中的 `...` 會使用 [`RevPrint(rbuf, ...)`](AlNum.md#revprint) 建立輸出字串。
* 可直接在 `if ()` 之後使用:
``` c++
   if (err)
      fon9_LOG_ERROR("InMyFounction|err=", err);
   else
      fon9_LOG_TRACE("InMyFounction|success");
```

## 使用範例
``` c++
#include "fon9/Log.hpp"
#include "fon9/LogFile.hpp"
#include <iostream>

int main() {
   auto res = fon9::InitLogWriteToFile("./logs/{0:f+8}.{1:04}.log", fon9::TimeChecker::TimeScale::Day, 1024*1024*1024, 0);
   if (!res) {
      std::cout << "Log file open fail: " << res.GetError().value() << ':' << res.GetError().message() << std::endl;
   }

   fon9_LOG_INFO("Program start.");
   // do something.
   fon9_LOG_INFO("Program done.");
}
```

log檔內容範例
```
$ cat ./logs/20180412.0000.log 
20180412-123444.968081  2624[IMP  ]LogFile.OnStart|orig=./logs/20180412.0000.log|time+8=20180412123444.968081
20180412-123444.968044  2625[IMP  ]TimerThread.ThrRun|name=Default.TimerThread
20180412-123444.968130  2624[INFO ]Program start.
20180412-123445.968215  2626[IMP  ]MessageQueueService.ThrRun|name=fon9.DefaultThreadPool.1
20180412-123446.968295  2624[INFO ]Program done.
20180412-123446.968486  2626[IMP  ]MessageQueueService.ThrRun.End|name=fon9.DefaultThreadPool.1
20180412-123446.968664  2624[IMP  ]LogFile.OnDestroy|orig=./logs/20180412.0000.log|time+8=20180412123444.968081
```
