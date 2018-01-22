# fon9 library - 事件通知機制: Subr
[fon9/Subr.hpp](../fon9/Subr.hpp)
---------------------------------------
## 命名、相同的概念(不同的名稱)
libfon9 採用 Subject 的概念來設計、命名。  
Subr = (Sub)ject + Subscribe(r)

* Observer
* Signal/Slot
  * boost signal(thread unsafe), signal2(thread safe)
  * Qt
* Event/Callback
  * C# event
* Subject/Subscribe
---------------------------------------
## 事件主題 `Subject<SubscriberT,MutexT,SubrMapT>`
#### SubscriberT
SubscriberT 有各種各樣的實現方式，例如：
* std::function<>
* function pointer
* interface (class + virtual function)
* 其他我沒想到的...

各有其優缺點，libfon9 需要選邊站嗎?
還是交給使用者視實際的應用，去取捨去決定吧!

#### MutexT
* 如果不需要 thread safe，可使用 [`fon9::DummyMutex`](ThreadTools.md#fon9dummymutex)。
* 使用 `std::mutex` 則是 thread safe，但是在收到發行訊息時，禁止取消註冊。
* 使用 `std::recursive_mutex` 則是 thread safe，在收到發行訊息時，可以取消註冊。
  * 收到發行訊息時，允許重進入發行，但不允許：在重進入發行時取消註冊。
  * 重進入發行的例子：在設計 桌面UI 時，如果某個 Click event 觸發了 `ProcessMessages()` 的呼叫，
    在 `ProcessMessages()` 返回前，可能會再觸發相同的 Clieck event。

#### SubrMapT
儲存 Subscriber 的容器，預設使用 `fon9::SortedVector`，這是 libfon9 能這麼快的秘密武器。

---------------------------------------
## `Subject` 主要介面
#### `Subscribe()` 註冊訂閱
```c++
template <class... ArgsT>
SubConn Subscribe(ArgsT&&... args);
```
* 必須可以用 args 建構一個 Subscriber。
* 如果 Subscriber 不支援 `bool operator==(const Subscriber& rhs) const` 則僅能使用 SubConn 來取消註冊。
* 如果 Subscriber 支援 `bool operator==(const Subscriber& rhs) const` 則可用 Subscriber 來取消註冊。

#### `Unsubscribe()` 取消訂閱
* `size_type Unsubscribe(SubConn connection);`
* `size_type Unsubscribe(const SubscriberT& subr);`
* `size_type UnsubscribeAll(const SubscriberT& subr);`

#### `Publish()` 發行訊息, 拋棄返回值
```c++
template <class... ArgsT>
void Publish(ArgsT&&... args);
```
呼叫 `Subscriber::operator()(std::forward<ArgsT>(args)...);`

#### `Combine()` 收集返回值, 可中斷發行流程
```c++
template <class CombinerT, class... ArgsT>
void Combine(CombinerT& combiner, ArgsT&&... args);
```
* 由發行訊息的人決定：要如何處理 Subscriber 的返回值。
* 由發行訊息的人決定：是否允許「在某個訂閱者返回後」結束發行 (放棄後續的訂閱者)。
* 可以利用 combiner 來呼叫不同的 member function：
  ```c++
  template <class MySubject, class... ArgsT>
  void MyPublish(MySubject& subj, ArgsT&&... args)
     struct Combiner {
        bool operator()(typename MySubject::Subscriber& subr, ArgsT&&... args) {
           subr->OnMessage(std::forward<ArgsT>(args)...);
           return true; // 返回 true:繼續處理下一個 subr; false:結束 MyPublish().
        }
     } combiner;
     subj.Combine(combiner, std::forward<ArgsT>(args)...);
  }
  ```

---------------------------------------
## `SubScopedConn` 自動取消訂閱
目前沒需求，所以還沒做。

---------------------------------------
## benchmark
* fon9/Subr_UT.cpp
  * Hardware: HP ProLiant DL380p Gen8 / E5-2680 v2 @ 2.80GHz
  * OS: ESXi 6.5.0 Update 1 (Build 5969303) / VM: Windows server 2016(1607)  
    Compiler: MSVC 2015(VC 19.00.24215.1)
```
function pointer                           : 0.071102694 secs / 10,000,000 times =   7.110269400 ns
unsafe.Subject<FnPtr>                      : 0.093822826 secs / 10,000,000 times =   9.382282600 ns
unsafe.Subject<std::function(FnPtr)>       : 0.095648106 secs / 10,000,000 times =   9.564810600 ns
unsafe.Subject<struct Subr>                : 0.094715487 secs / 10,000,000 times =   9.471548700 ns
unsafe.Subject<std::function(struct Subr)> : 0.094584979 secs / 10,000,000 times =   9.458497900 ns
Subject<FnPtr>                             : 0.430092224 secs / 10,000,000 times =  43.009222400 ns
Subject<std::function(FnPtr)>              : 0.442030412 secs / 10,000,000 times =  44.203041200 ns
Subject<struct Subr>                       : 0.387816413 secs / 10,000,000 times =  38.781641300 ns
Subject<std::function(struct Subr)>        : 0.443034517 secs / 10,000,000 times =  44.303451700 ns
```
  * OS: ESXi 6.5.0 Update 1 (Build 5969303) / VM: ubuntu16 4.4.0-62-generic  
    Compiler: g++ (Ubuntu 5.4.0-6ubuntu1~16.04.4) 5.4.0 20160609
```
function pointer                           : 0.071520543 secs / 10,000,000 times =   7.152054300 ns
unsafe.Subject<FnPtr>                      : 0.094122920 secs / 10,000,000 times =   9.412292000 ns
unsafe.Subject<std::function(FnPtr)>       : 0.094315276 secs / 10,000,000 times =   9.431527600 ns
unsafe.Subject<struct Subr>                : 0.070378092 secs / 10,000,000 times =   7.037809200 ns
unsafe.Subject<std::function(struct Subr)> : 0.092347553 secs / 10,000,000 times =   9.234755300 ns
Subject<FnPtr>                             : 0.333777316 secs / 10,000,000 times =  33.377731600 ns
Subject<std::function(FnPtr)>              : 0.320089850 secs / 10,000,000 times =  32.008985000 ns
Subject<struct Subr>                       : 0.321531892 secs / 10,000,000 times =  32.153189200 ns
Subject<std::function(struct Subr)>        : 0.319457887 secs / 10,000,000 times =  31.945788700 ns
```
* [signal slot benchmarks](https://github.com/NoAvailableAlias/signal-slot-benchmarks/tree/master/#performance)
  * [點這兒看結果](../ext/sigslot/README.md)