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
## 一般工具/演算法/容器
### [Signal/Slot (Observer, Subject/Subscriber, Event, Callback...)](Overview/Subr.md)
### [SortedVector](fon9/SortedVector.hpp)
### Log
### [Trie](https://zh.wikipedia.org/wiki/Trie)
---------------------------------------
## [Thread 工具](Overview/ThreadTools.md)
---------------------------------------
## 文字/數字/基礎型別
### [StrView](fon9/StrView.hpp)
  * 雖然 C++17 已納入 std::string_view，但不符合我的需求。例如，沒有提供最常用的建構：  
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

### Decimal：使用「整數 + 小數長度」的型式來表達浮點數
  * 因交易系統對小數精確度的要求，無法使用 double，即使 long double 仍有精確度問題，
    所以必須自行設計一個「可確定精確度」的型別。
### TimeStamp
### Format / RevBuffer / Buffer 機制
  * 類似的 lib: [{fmt} library](http://zverovich.net/2013/09/07/integer-to-string-conversion-in-cplusplus.html)
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
