libfon9 基礎建設程式庫
=======================

## 基本說明
* libfon9 是 **風言軟體工程有限公司** 獨力開發的「C++ 跨平台基礎建設」程式庫
* 使用 C++11
  * Windows: VS 2015
    * `./build/vs2015/fon9.sln`
  * Linux:
    * gcc (Ubuntu 5.4.0-6ubuntu1~16.04.4) 5.4.0 20160609
    * cmake version 3.5.1
    * `./build/cmake/build.sh`
* 跨平台，但不支援老舊的OS
* 僅考慮 64 位元平台
* 字串僅考慮 UTF8，source file 也使用 UTF8 編碼。
  * MSVC: 已在 [`fon9/sys/Config.h`](fon9/sys/Config.h) 裡面設定: `#pragma execution_character_set("UTF-8")`
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
  * 許多第3方的 library，速度很快、功能強大，但也遠超過我的需求。
  * 我個人傾向於：設計到剛好滿足自己的需求就好。

### [準備工作](Overview/Prepare.md)

---------------------------------------

## libfon9 Topics
有空時也許可以針對底下的主題撰文介紹。

### [Signal/Slot](Overview/Subr.md)
* 這是首先釋出的項目。
* 因為此機制最獨立、單純，與其他主題的關聯性最小。
* 我們可以從這裡著手，把基本的開發環境建立起來。
* 而且已經有一個很棒的效能測試 [signal slot benchmarks 測試方法](https://github.com/NoAvailableAlias/signal-slot-benchmarks)。

### 演算法/容器
* [SortedVector](fon9/SortedVector.hpp)
* [SinglyLinkedList](fon9/SinglyLinkedList.hpp)
* [Trie](Overview/Trie.md)

### [文數字轉換](Overview/AlNum.md)
* 產出資料的 [串列 Buffer 機制](fon9/buffer)
* 包含文數字轉換
* [格式化字串](Overview/AlNum.md#格式化字串)，類似的 library: [{fmt} library](https://github.com/fmtlib/fmt)

### [Thread Tools](Overview/ThreadTools.md)
* [Thread Timer](Overview/ThreadTools.md#timer-計時器)
* [DefaultThreadPool](Overview/ThreadTools.md#fon9getdefaultthreadpool)
* [其他...](Overview/ThreadTools.md)

### [File/Serialize/Deserialize](Overview/Filing.md)

### [Logging](Overview/Logging.md)
* 類似的 library: [spdlog](https://github.com/gabime/spdlog)
* [效率測試及比較](ext/logvs)

### [通訊基礎建設](Overview/IO.md)

### [系統管理基礎建設](Overview/Manage.md)

### Simple FIX engine

### [雜項](Overview/Misc.md)
