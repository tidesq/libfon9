fon9 library - prepare
=======================

## 主要開發工具及版本
* Windows: VS 2015
  * `./build/vs2015/fon9.sln`
* Linux:
  * cmake version 3.5.1
  * gcc (Ubuntu 5.4.0-6ubuntu1~16.04.4) 5.4.0 20160609
  * `./build/cmake/build.sh`
* 開啟 compiler 全部的警告訊息：警告訊息的重要性，相信不用再提醒了。
  * 警告訊息 -- 零容忍。

## 預設路徑結構
```
~/devel/                我的開發環境根目錄
  |-- external/         third-party libraries
  |
  |-- output/fon9/      libfon9 的輸出路徑，「輸出路徑」移到原始程式路徑之外，可以更容易「乾淨備份」。
  |   |-- 64/Release/   預設 Windows 的 fon9.sln 輸出路徑
  |   \-- release/fon9/ 預設 Linux 輸出路徑
  |
  \-- fon9/
      |-- fon9/         header(.hpp & .h) & source(.cpp & .c) 放在一起，更容易找到所需的檔案。
      |   \-- sys/      平台、Compiler 相關
      |
      |-- ext/          配合 third-party 寫的測試或 benchmark
      |   |-- sigslot/  signal-slot-benchmarks
      |   \-- logvs/    logging 測試與比較
      |
      \-- build/
          |-- cmake/    build shell for cmake
          \-- vs2015/   VS2015 project & solution files
```
