libfon9 基礎建設程式庫
=======================

## 基本說明
* libfon9 是 **風言軟體工程有限公司** 獨力開發的「C++ 跨平台基礎建設」程式庫
* 使用 C++11
  * Windows: VS 2015
  * Linux:
    * gcc (Ubuntu 5.4.0-6ubuntu1~16.04.4) 5.4.0 20160609
    * cmake version 3.5.1
* 跨平台，但不支援老舊的OS
* 僅考慮 64 位元平台
* 字串僅考慮 UTF8，source file 也使用 UTF8 編碼。
  * MSVC: 已在 [fon9/sys/Config.h](fon9/sys/Config.h) 裡面設定: `#pragma execution_character_set("UTF-8")`
  * GCC: 預設值已是 UTF-8, 或可強制使用: `-fexec-charset=UTF-8` 參數

在眾多 open source 的情況下，為何還要寫？
* 追求速度
  * std 、 boost 非常好，但速度不是他們追求的。
  * 當 **速度** 與 **通用性** 需要取捨時，libfon9 選擇 **速度**。
  * 當 **速度** 與 **RAM用量** 需要取捨時，libfon9 選擇 **速度**。
    * libfon9 會考慮到 大量的RAM 造成 CPU cache missing，速度不一定會變快。
  * 速度問題分成2類: Low latency 、 High throughtput，當2者有衝突時，盡量選擇 **Low latency**。
* 降低第3方的依賴。
* 更符合自己的需求。
  * 許多第3方的 lib，速度很快、功能強大，但也遠超過我的需求。
  * 我個人傾向於：設計到剛好滿足自己的需求就好。

### [準備工作](prepare.md)

---------------------------------------
## 演算法/容器
### [SortedVector](fon9/SortedVector.hpp)
### [SinglyLinkedList](fon9/SinglyLinkedList.hpp)
### [Trie](https://zh.wikipedia.org/wiki/Trie)
---------------------------------------
## 一般工具
### [Signal/Slot (Observer, Subject/Subscriber, Event, Callback...)](Overview/Subr.md)
### [Buffer 機制](fon9/buffer)
### Log
* [fon9/Log.hpp](fon9/Log.hpp)
* `fon9_LOG_TRACE(...)`、`fon9_LOG_DEBUG(...)`、`fon9_LOG_INFO(...)`...
* 其中的 `...` 會使用 `RevPrint(rbuf, ...)` 建立訊息。
### StaticPtr
* [fon9/StaticPtr.hpp](fon9/StaticPtr.hpp)
* 取代 `static std::unique_ptr<T> ptr;` 或 `static thread_local std::unique_ptr<T> ptr;`
* 因為在 ptr 死亡後，可能還會用到 ptr。
* 增加 ptr.IsDisposed() 判斷 ptr 本身(不是所指物件)，是否已經死亡。
### intrusive_ptr<>、intrusive_ref_counter<>
* [fon9/intrusive_ptr.hpp](fon9/intrusive_ptr.hpp)
  * [參考 boost 的文件](http://www.boost.org/doc/libs/1_60_0/libs/smart_ptr/intrusive_ptr.html)
  * fon9 的額外調整: 在 intrusive_ptr_release() 時, 如果需要刪除, 則會呼叫 intrusive_ptr_deleter();
    * 如此一來就可以自訂某型別在 intrusive_ptr<> 的刪除行為
    * 預設直接呼叫 delete
    * 例:
      ```c++
      class MyClass : public fon9::intrusive_ref_counter<MyClass> {
         virtual void OnBeforeDestroy() const {
         }
         // 自訂 intrusive_ptr<> 刪除 MyClass 的方法.
         inline friend void intrusive_ptr_deleter(const MyClass* p) {
            p->OnBeforeDestroy();
            delete p;
         }
      };
      ```
* [fon9/intrusive_ref_counter.hpp](fon9/intrusive_ref_counter.hpp)
  * [參考 boost 的文件](http://www.boost.org/doc/libs/1_60_0/libs/smart_ptr/intrusive_ref_counter.html)
---------------------------------------
## 文字/數字/基礎型別
### [StrView](fon9/StrView.hpp)
  * 雖然 C++17 已納入 `std::string_view`，但不符合我的需求。例如，沒有提供最常用的建構：  
    `template <size_t sz> StrView(const char (&cstr)[sz])`
### [StrTo](fon9/StrTo.hpp) 字串轉數字
  * 額外提供 `fon9::isspace()`, `fon9::isalpha()`... [StrTools.hpp](fon9/StrTools.hpp)，因為標準函式庫的速度太慢。  
    而 fon9 只考慮 UTF-8(ASCII) 所以速度可以更快。
  * 底下是使用 [fon9/StrTo_UT.cpp](fon9/StrTo_UT.cpp) 測試的部分結果
  * Hardware: HP ProLiant DL380p Gen8 / E5-2680 v2 @ 2.80GHz
  * OS: ESXi 6.5.0 Update 1 (Build 5969303) / VM: Windows server 2016(1607)  
    Compiler: MSVC 2015(VC 19.00.24215.1)
```
::isspace()      : 0.046315328 secs / 1,000,000 times = 46.315328000  ns
std::isspace()   : 0.045184748 secs / 1,000,000 times = 45.184748000  ns
fon9::isspace()  : 0.001268421 secs / 1,000,000 times = 1.268421000   ns
--- int64/imax ---
NaiveStrToSInt() : 0.009116134 secs / 1,000,000 times = 9.116134000   ns
StrToNum(int64)  : 0.023595196 secs / 1,000,000 times = 23.595196000  ns
strtoimax()      : 0.080947254 secs / 1,000,000 times = 80.947254000  ns
```
  * OS: ESXi 6.5.0 Update 1 (Build 5969303) / VM: ubuntu16 4.4.0-62-generic  
    Compiler: g++ (Ubuntu 5.4.0-6ubuntu1~16.04.4) 5.4.0 20160609
```
::isspace()      : 0.002872489 secs / 1,000,000 times = 2.872489000   ns
std::isspace()   : 0.003229382 secs / 1,000,000 times = 3.229382000   ns
fon9::isspace()  : 0.000490955 secs / 1,000,000 times = 0.490955000   ns
--- int64/imax ---
NaiveStrToSInt() : 0.009790892 secs / 1,000,000 times = 9.790892000   ns
StrToNum(int64)  : 0.014731447 secs / 1,000,000 times = 14.731447000  ns
strtoimax()      : 0.039363580 secs / 1,000,000 times = 39.363580000  ns
```
### [ToStr](fon9/ToStr.hpp) 數字轉字串
  * 底下是使用 [fon9/ToStr_UT.cpp](fon9/ToStr_UT.cpp) 測試的部分結果
  * Hardware: HP ProLiant DL380p Gen8 / E5-2680 v2 @ 2.80GHz
  * OS: ESXi 6.5.0 Update 1 (Build 5969303) / VM: Windows server 2016(1607)
```
ToStrRev(long)   : 0.006805115 secs / 1,000,000 times =   6.805115000 ns
_ltoa(long)      : 0.020169359 secs / 1,000,000 times =  20.169359000 ns
to_string(long)  : 0.023836049 secs / 1,000,000 times =  23.836049000 ns
sprintf(long)    : 0.159748090 secs / 1,000,000 times = 159.748090000 ns
RevFormat()      : 0.031855329 secs / 1,000,000 times =  31.855329000 ns
ToStrRev(Fmt)    : 0.016562426 secs / 1,000,000 times =  16.562426000 ns
```
  * OS: ESXi 6.5.0 Update 1 (Build 5969303) / VM: ubuntu16 4.4.0-62-generic
```
ToStrRev(long)   : 0.005728993 secs / 1,000,000 times =   5.728993000 ns
to_string(long)  : 0.086369344 secs / 1,000,000 times =  86.369344000 ns
sprintf(long)    : 0.075958616 secs / 1,000,000 times =  75.958616000 ns
RevFormat()      : 0.026977021 secs / 1,000,000 times =  26.977021000 ns
ToStrRev(Fmt)    : 0.015506810 secs / 1,000,000 times =  15.506810000 ns
```
### [Decimal](fon9/Decimal.hpp)：使用「整數 + 小數長度」的型式來表達浮點數
  * 因交易系統對小數精確度的要求，無法使用 double，即使 long double 仍有精確度問題，
    所以必須自行設計一個「可確定精確度」的型別。
  * 例: `Decimal<int64_t, 6> v;` 使用 int64_t 儲存數值，小數 6 位。
    * 如果 `v.GetOrigValue() == 987654321` 則表示的數字是: `987.654321`
### [TimeInterval](fon9/TimeInterval.hpp) / [TimeStamp / TimeZoneOffset](fon9/TimeStamp.hpp)：時間處理機制
  * 取得現在 UTC 時間：`fon9::UtcNow();`
  * 取得現在 本地時間：`fon9::TimeStamp lo = fon9::UtcNow() + fon9::GetLocalTimeZoneOffset();`
### Format 機制
  * 類似的 lib: [{fmt} library](http://zverovich.net/2013/09/07/integer-to-string-conversion-in-cplusplus.html)
  * 基本格式化輸出
    * `ToStrRev(pout, value, fmt);`
    * `RevPrint(RevBuffer& rbuf, value1, value2, fmt2, ...);`
      * [fon9/RevPrint.hpp](fon9/RevPrint.hpp)
      * value1 無格式化, 轉呼叫 `ToStrRev(pout, value1);`
      * value2 使用 fmt2 格式化, 轉呼叫 `ToStrRev(pout, value2, fmt2);`
  * 格式化輸出, 類似 `sprintf();`, `fmt::format()`
      * `RevFormat(rbuf, format, value...);`
        * [fon9/RevFormat.hpp](fon9/RevFormat.hpp)
      * 例如:
      ```c++
      // output: "/abc/def/123"
      void test(fon9::RevBuffer& rbuf) {
         fon9::RevFormat(rbuf, "{0}", 123);
         fon9::RevFormat(rbuf, "/{0:x}/{1}/", 0xabc, fon9::ToHex{0xdef});
      }
      ```
---------------------------------------
## [Thread 工具](Overview/ThreadTools.md)
### [MustLock](Overview/ThreadTools.md#fon9mustlock)
### [DummyMutex](Overview/ThreadTools.md#fon9dummymutex)
### [CountDownLatch](Overview/ThreadTools.md#fon9countdownlatch)
### [CyclicBarrierLatch](Overview/ThreadTools.md#fon9cyclicbarrier)
### [ThreadController](Overview/ThreadTools.md#fon9threadcontrollerprotectedt-waitpolicy)
### [Timer 計時器](Overview/ThreadTools.md#timer-計時器)
### [MessageQueue](Overview/ThreadTools.md#messagequeue)
---------------------------------------
## 檔案/儲存/載入
### File/Path tools
### Serialize/Deserialize
### InnFile
### 資料同步機制
---------------------------------------
## 系統管理工具
### 系統管理基礎建設 (Seed 機制)
### User Auth 使用者管理機制
  * SASL (SCRAM-SHA-1)
---------------------------------------
## 通訊基礎建設
---------------------------------------
## Simple FIX engine
