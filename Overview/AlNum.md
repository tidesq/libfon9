libfon9 基礎建設程式庫 - 文數字轉換
===================================

## 基本說明
* 僅考慮 UTF8
* 僅考慮從 string 轉 value，或從 value 轉 string。不考慮 io stream。

## 文字/數字/基礎型別
### StrView
* [`fon9/StrView.hpp`](../fon9/StrView.hpp)
* 雖然 C++17 已納入 `std::string_view`，但不符合我的需求。例如，沒有提供最常用的建構：  
  `template <size_t sz> StrView(const char (&cstr)[sz])`

### StrTo 字串轉數字
* 基本轉換函式，詳細說明請參考 [`fon9/StrTo.hpp`](../fon9/StrTo.hpp)。
``` c++
template <class AuxT>
const char* StrToNum(StrView str, AuxT& aux, typename AuxT::ResultT& out);

template <typename NumT, class AuxT = AutoStrToAux<NumT>>
NumT StrTo(const StrView& str, NumT value = NumT{}, const char** endptr = nullptr);
```
* 額外提供 `fon9::isspace()`, `fon9::isalpha()`... [`fon9/StrTools.hpp`](../fon9/StrTools.hpp)，因為標準函式庫的速度太慢。  
  而 fon9 只考慮 UTF-8(ASCII) 所以速度可以更快。
* 底下是使用 [`fon9/StrTo_UT.cpp`](../fon9/StrTo_UT.cpp) 測試的部分結果
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

### ToStr 數字轉字串
* 基本轉換函式，詳細說明請參考[`fon9/ToStr.hpp`](../fon9/ToStr.hpp)。
``` c++
/// 建立 value 的 10 進位字串.
/// - 從 (*--pout) 往前填入.
/// - 傳回最後一次 pout 位置: 字串開始位置.
template <typename IntT>
char* ToStrRev(char* pout, IntT value);
``` 
* 如果不確定要多少 buffer，則使用 `fon9::NumOutBuf` 會是比較保險的做法
``` c++
   fon9::NumOutBuf nbuf;
   nbuf.SetEOS();
   std::cout << fon9::ToStrRev(nbuf.end(), value);
```
* 指定格式轉換函式，詳細說明請參考[`fon9/ToStrFmt.hpp`](../fon9/ToStrFmt.hpp)，[`fon9/FmtDef.hpp`](../fon9/FmtDef.hpp)。
  * 如果 fmt 為動態產生，無法預知最大可能寬度，則應使用 [`RevPrint()`](AlNum.md#revprint) 才是安全的做法。
``` c++
template <typename IntT>
char* ToStrRev(char* pout, IntT value, const FmtDef& fmt);
``` 
* 底下是使用 [`fon9/ToStr_UT.cpp`](../fon9/ToStr_UT.cpp) 測試的部分結果
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

### Decimal：使用「整數 + 小數長度」的型式來表達浮點數
* [`fon9/Decimal.hpp`](../fon9/Decimal.hpp)
* 因交易系統對小數精確度的要求，無法使用 double，即使 long double 仍有精確度問題，
  所以必須自行設計一個「可確定精確度」的型別。
* 例: `Decimal<int64_t, 6> v;` 使用 int64_t 儲存數值，小數 6 位。
  * 如果 `v.GetOrigValue() == 987654321` 則表示的數字是: `987.654321`
* 轉字串依然使用 `ToStrRev()`。
* 字串轉 Decimal: 依然使用 `ResultT StrTo(const StrView& str, ResultT value = ResultT::Null(), const char** endptr = nullptr)`，
  其中的 `ResultT` 為 `Decimal<>` 型別。

### 時間處理
* 時間間隔 [`fon9/TimeInterval.hpp`](../fon9/TimeInterval.hpp)
* 時間戳、時區 [`fon9/TimeStamp.hpp`](../fon9/TimeStamp.hpp)
* 取得現在 UTC 時間：`fon9::UtcNow();`
* 取得現在 本地時間：`fon9::TimeStamp lo = fon9::UtcNow() + fon9::GetLocalTimeZoneOffset();`
* 轉字串: `char* ToStrRev(char* pout, TimeStamp ts, const FmtTS& fmt);`   
  其中的 `FmtTS` 如果使用字串建構 `FmtTS(StrView fmtstr)`:
  * fmtstr: `[TsFmtItemChar[ch]][[+-TimeZoneOffset(spc)][width][.precision]]`
  * `TimeZoneOffset` 若使用 `TimeZoneName` 則必須加上「'」單引號，例如： `'TW'`
  * 如果尾端有 "." 或 ".0" 則自動小數位, 如果尾端沒有 ".precision" 則不顯示小數位
  * 其中的 `TsFmtItemChar`:
      ``` c++
      enum class TsFmtItemChar : char {
         YYYYMMDDHHMMSS = 'L',

         YYYYMMDD = 'f',
         YYYY_MM_DD = 'F',
         YYYYsMMsDD = 'K',
         HHMMSS = 't',
         HH_MM_SS = 'T',

         Year4 = 'Y',
         Month02 = 'm',
         Day02 = 'd',

         Hour02 = 'H',
         Minute02 = 'M',
         Second02 = 'S',
      };
      ```
  * ch: 除了 `TsFmtItemChar` 之外的字元，保留不變，原樣輸出。
  * 範例:
    * "L" 輸出 "20180412144156"
    * "f" 輸出 "20180412"
    * "F" 輸出 "2018-04-12"
    * "K" 輸出 "2018/04/12"
    * "K-T" 輸出 "2018/04/12-14:41:56"

---------------------------------------

## 格式化字串
### RevPrint
* [`fon9/RevPrint.hpp`](../fon9/RevPrint.hpp)
* 基本格式化字串: `RevPrint(RevBuffer& rbuf, value1, value2, fmt2, ...);`
  * value1 無格式化，轉呼叫 `ToStrRev(pout, value1);`
  * value2 使用 fmt2 格式化，轉呼叫 `ToStrRev(pout, value2, fmt2);`
* 格式化字串傳回 `StrT` 可以是 `std::string`
  * `template <class StrT, class... ArgsT>  StrT& RevPrintAppendTo(StrT& dst, ArgsT&&... args);`
  * `template <class StrT, class... ArgsT>  StrT RevPrintTo(ArgsT&&... args);`

### RevFormat
* [`fon9/RevFormat.hpp`](../fon9/RevFormat.hpp)
* 類似的 library: [{fmt} library](https://github.com/fmtlib/fmt)
* 格式化字串, 類似 `sprintf();`, `fmt::format()`
  * `RevFormat(rbuf, format, value...);`
    * format: `"{0:fmt0} {1:fmt1} {3}..."`
    * 其中大括號裡面的參數Id從0開始，不可省略。
    * 一般數字、字串的 fmt 參數格式 `[flags][width][.precision]` 定義, 與 [`printf` 的 format](http://www.cplusplus.com/reference/cstdio/printf/) 完全一樣
  * 例如:
    ```c++
    // output: "/abc/def/123"
    void test(fon9::RevBuffer& rbuf) {
       fon9::RevFormat(rbuf, "{0}", 123);
       fon9::RevFormat(rbuf, "/{0:x}/{1}/", 0xabc, fon9::ToHex{0xdef});
    }
    ```
  * 預先格式化，如果某個格式很常用到，可預先將格式解析好：
    ```c++
    static FmtPre msgfmt{"{0} {1} {2}"};
    msgfmt(rbuf, "Hello", "World", '!');
    ```
