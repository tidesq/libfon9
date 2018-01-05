# fon9 library

## 前言
我計畫在 2018年 逐步整理、公開 libfon9 目前的成果，libfon9 是為了開發一個 **交易系統** 所需的基礎建設。
* 使用 C++11
* 跨平台，但不支援老舊的OS
* 僅考慮 64 位元平台
* 字串僅考慮 UTF8

由於我書念得不多、學識有限、且沒受過正規的電腦科學教育，所有在這兒公開的東西，也許早就有研究成果
(例如：[Trie](https://en.wikipedia.org/wiki/Trie) 早在 1959年 就已發表，我卻無知到 2017/12/28 才知道這事)，
或早已有優秀的 open source。

那我為何還要自己寫？
* 追求速度
  * std 、 boost 非常好，但速度不是他們追求的。
  * 當 **速度** 與 **通用性** 需要取捨時，libfon9 選擇 **速度**。
  * 當 **速度** 與 **RAM用量** 需要取捨時，libfon9 選擇 **速度**。
    * libfon9 會考慮到 大量的RAM 造成 CPU cache missing，速度不一定會變快。
  * 速度問題分成2類: Low latency 、 High throughtput，當2者有衝突時，盡量選擇 **Low latency**。
* 降低第3方的依賴。
* 更符合自己的需求。
  * 許多第3方的 lib，速度很快、功能強大，但也遠超過我的需求。
  * 我個人傾向於：設計到剛好滿足自己的需求。

## 預計公開的主題
### 一般工具/演算法
* Log 機制
* SortedVector
* [Signal/Slot (Observer, Subject/Subscriber, Event, Callback...)](Overview/Subr.md)
* [Trie](https://zh.wikipedia.org/wiki/Trie)
  * 目前取名為 fonix::LevelMap，底下是針對「委託書號」的需求，比較 std::map, std::unordered_map 的結果  
    key = "xxxxx", x='0'..'9', 'A'..'Z', 'a'..'z'  
    測試的範圍: "00000".."04C91" 共 1,000,000 筆。
```
Hardware: HP ProLiant DL380p Gen8 / E5-2680 v2 @ 2.80GHz
OS: ESXi 6.5.0 Update 1 (Build 5969303) / VM: Windows server 2016(1607)
Compiler: MSVC 2015(VC 19.00.24215.1)
+--------------+------------+-----------------+--------------------+
|value = ns/op | std::map   | fonix::LevelMap | std::unordered_map |
+--------------+------------+-----------------+--------------------+
| emplace      | 1065.395   | 96.301          | 161.856            |
| find         |  585.563   | 36.564          |  77.885            |
| lower_bound  |  617.089   | 34.807          |  77.724            |
| upper_bound  |  560.925   | 55.318          |  76.962            |
| clear        |   64.238   | 22.483          |  63.679            |
+--------------+------------+-----------------+--------------------+
| MemUsed      | 85,024,768 | 71,798,784      | 144,584,704        |
+--------------+------------+-----------------+--------------------+
PS. VC 的 std::unordered_map 有提供 lower_bound() ?!
```
```
OS: ESXi 6.5.0 Update 1 (Build 5969303) / VM: ubuntu16 4.4.0-62-generic
Compiler: g++ (Ubuntu 5.4.0-6ubuntu1~16.04.4) 5.4.0 20160609
+--------------+------------+-----------------+--------------------+
|value = ns/op | std::map   | fonix::LevelMap | std::unordered_map |
+--------------+------------+-----------------+--------------------+
| emplace      | 556.259    | 44.061          | 70.314             |
| find         | 297.138    | 16.957          | 36.370             |
| lower_bound  | 310.717    | 14.002          | -                  |
| upper_bound  | 287.250    | 21.781          | -                  |
| clear        |  38.129    |  6.022          | 20.013             |
+--------------+------------+-----------------+--------------------+
```
* LevelArray
* 瞭解您要處理的資料，選擇適合的演算法，是非常重要的事!

### Thread 工具
* MustLock
* ThreadPool
* Timer

### 文字/數字/基礎型別
* StrView
  * 雖然 C++17 已納入 std::string_view，但不符合我的需求。例如，沒有提供最常用的建構：  
    `template <size_t sz> StrView(const char (&cstr)[sz])`
* Decimal：使用「整數 + 小數長度」的型式來表達浮點數
  * 因交易系統對小數精確度的要求，無法使用 double，即使 long double 仍有精確度問題，
    所以必須自行設計一個「可確定精確度」的型別。
* TimeStamp
* Format / RevBuffer / Buffer機制
  * 類似的 lib: [{fmt} library](http://zverovich.net/2013/09/07/integer-to-string-conversion-in-cplusplus.html)
* StrTo
* StrParser

### 檔案/儲存/載入
* File/Path tools
* Serialize/Deserialize
* InnFile
* 資料同步機制

### 通訊機制
* 通訊基礎建設

### 系統管理
* 系統管理基礎建設 (Seed 機制)
* User Auth 使用者管理機制
  * SASL (SCRAM-SHA-1)

### Simple FIX engine
