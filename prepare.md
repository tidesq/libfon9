# fon9 library - prepare

## 主要開發工具及版本
* Windows: VS 2015
* Linux:
  * cmake version 3.5.1
  * gcc (Ubuntu 5.4.0-6ubuntu1~16.04.4) 5.4.0 20160609
  * clang version 3.8.0-2ubuntu4 (tags/RELEASE_380/final)

## 預設路徑結構
```
~/devel/                我的開發環境根目錄
  |-- external/         third-party libraries
  |-- release/          install path, build type = "release"
  |-- output/           libfon9 的輸出路徑，「輸出路徑」移到原始程式路徑之外，可以更容易「乾淨備份」。
  |   \-- release/fon9/
  |
  \-- fon9/
      |-- fon9/         header(.hpp & .h) & source(.cpp & .c) 放在一起，更容易找到所需的檔案。
      |   \-- sys/      平台、Compiler 相關
      |
      |-- ext/          配合 third-party 寫的測試或 benchmark
      |   \-- sigslot/  signal-slot-benchmarks
      |
      \-- build/
          |-- cmake/    build shell for cmake
          \-- vs2015/   VS2015 project & solution files
```

## 準備工作
* 開啟 compiler 全部的警告訊息：警告訊息的重要性，相信不用再提醒了。
  * 警告訊息 -- 零容忍。

## Signal/Slot
首先要釋出的是 [Signal/Slot](Overview/Subr.md) 機制。
* 因為此機制最獨立、單純，與其他主題的關聯性最小。
* 我們可以從這裡著手，把基本的開發環境建立起來。
* 而且已經有一個很棒的效能測試 [signal slot benchmarks](https://github.com/NoAvailableAlias/signal-slot-benchmarks/tree/master/#performance)。
