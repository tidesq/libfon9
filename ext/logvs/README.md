# fon9 log

## 簡單的結果紀錄及思考
http://fonwin.blogspot.tw/2018/04/c-logging-library-comparison.html

## [測試說明](https://docs.google.com/spreadsheets/d/17mGrTZJBR-xHcwqySYvliLJ6y65RJvC8qWiWO4Nb28U/edit#gid=1494936172)
* 從各種 log 的表現，思考不同機制對效能的影響:
  * lock-free、spinlock
  * lazy format：也就是說在呼叫 log 時，只將參數內容打包，丟到 queue，然後就返回。

* 測試環境及方法
  * Hardware: HP ProLiant DL380p Gen8 / E5-2680 v2 @ 2.80GHz
  * OS: ESXi 6.5.0 Update 1 (Build 5969303) / VM: ubuntu16 4.4.0-62-generic (分配 8 cores)
  * 測試 5 次，取平均值最低的那次。
  * 延遲數據使用所有 threads 合併後計算的結果。
  * 使用 /usr/bin/time -v 測量資源使用狀況。
  * 測試程式: `libfon9/ext/logvs/*`

* 比較對象:
  * [fon9](https://github.com/fonwin/libfon9)
    * 立即格式化，格式化之後的字串丟到 locked buffer，在另一 thread 寫檔。
    * lock 機制使用: `using Mutex = SpinMutex<YieldSleepPolicy>;`
      * 經過測試: 使用 SpinBusy 在競爭激烈時，延遲時間會快速惡化。
      * 經過測試: 使用 std::mutex 延遲時間會相對穩定，且每個 thread 會很平均，但與 YieldSleepPolicy 相比，延遲較高。
    * 大約 [每2萬筆] 或 [每秒] 觸發一次寫檔通知。
  * [spdlog](https://github.com/gabime/spdlog)
    * 使用 libfmt 立即格式化，格式化的程式在:
      * [spdlog/details/logger_impl.h](https://github.com/gabime/spdlog/blob/master/include/spdlog/details/logger_impl.h)
      * spdlog::logger::log(...)
    * 使用 lock-free bounded queue 處理 async_msg
    * 但是 async_msg 用了 2 個 std::string (這是成本很高, 但容易被忽視的物件)
        * [spdlog/details/async_log_helper.h](https://github.com/gabime/spdlog/blob/master/include/spdlog/details/async_log_helper.h)
        * `struct async_msg { std::string logger_name, txt; }`
        * 為了避免 std::string 造成效率的干擾，所以測試時把 `async_msg(const details::log_msg &m);` 建構，
          改成不填入 logger_name 及 txt，也就是沒有填入log訊息，因為我只想知道 lock-free bounded queue 到底有多大的威力。
    * `spdlog::set_async_mode(1048576);`
    * `spdlog::create<spdlog::sinks::simple_file_sink_mt>("file_logger", "/tmp/spd-async.txt", false);`
  * [mal(mini-async-log)](https://github.com/RafaGago/mini-async-log)
    * 使用 lazy format + lock-free (bounded queue) + (node based mpsc queue)。
    * [轉字串] & [寫 log 檔] 的工作放在獨立的 thread 裡面。
    * queue 使用 2 個機制 (mal_cfg.queue.can_use_heap_q = true;):
      * 在 log entry 小於 64B && bounded_queue 空間足夠，則直接放到 bounded_queue
      * 否則分配記憶體來儲存 log entry，然後存入 node base mpsc queue，   
        所以一旦單一 log entry 超過 64B，或瞬間資料量超過 bounded queue size，效能就會降低。
  * [NanoLog](https://github.com/Iyengar111/NanoLog)
    * 使用 lazy format + lock-free bounded queue。
    * 由於 mal 的效能比 NanoLog 好很多，所以 NanoLog 就不列入評比了。

## 壓力測試
https://docs.google.com/spreadsheets/d/17mGrTZJBR-xHcwqySYvliLJ6y65RJvC8qWiWO4Nb28U/edit#gid=0

## 慢速測試(冷系統)
https://docs.google.com/spreadsheets/d/17mGrTZJBR-xHcwqySYvliLJ6y65RJvC8qWiWO4Nb28U/edit#gid=381584878
