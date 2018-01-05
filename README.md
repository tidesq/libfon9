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
### SortedVector
### Log
### [Trie](https://zh.wikipedia.org/wiki/Trie)
---------------------------------------
## [Thread 工具](Overview/ThreadTools.md)
---------------------------------------
## 文字/數字/基礎型別
### StrView
  * 雖然 C++17 已納入 std::string_view，但不符合我的需求。例如，沒有提供最常用的建構：  
    `template <size_t sz> StrView(const char (&cstr)[sz])`
### Decimal：使用「整數 + 小數長度」的型式來表達浮點數
  * 因交易系統對小數精確度的要求，無法使用 double，即使 long double 仍有精確度問題，
    所以必須自行設計一個「可確定精確度」的型別。
### TimeStamp
### Format / RevBuffer / Buffer 機制
  * 類似的 lib: [{fmt} library](http://zverovich.net/2013/09/07/integer-to-string-conversion-in-cplusplus.html)
### StrTo
### StrParser
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
