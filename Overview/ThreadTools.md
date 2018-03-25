# fon9 library - Thread Tools

## `fon9::MustLock`
[fon9/MustLock.hpp](../fon9/MustLock.hpp)
* 當某個物件需要提供 thread safe 特性時，一般而言都需要用 mutex 來保護，通常寫法如下，
  但這樣的缺點非常明顯：每次要使用 `obj_` 時，都要「記得」使用 lock，一旦有地方遺漏了，就會造成難以發現的 bug。
  * 宣告:
    ```c++
    std::mutex mx_;
    MyObject   obj_;
    ```
  * 使用:
    ```c++
    std::unique_lock lk(mx_);
    obj_.foo();
    ```
* 所以 libfon9 設計了一個「強制要求」必須鎖住才能使用的機制：  
  `fon9::MustLock<BaseT,MutexT>`
  * 宣告：
    ```c++
    using MyThreadSafeObj = fon9::MustLock<MyObject,std::mutex>;
    MyThreadSafeObj obj_;
    ```
  * 使用：
    ```c++
    MyThreadSafeObj::Locker obj{obj_}; // 或 auto obj{obj_.Lock()};
    obj->foo(); // lock 之後才能使用, 此時使用任何一個 MyObject 的 member 都是 thread safe.
    ```
  * 缺點(副作用)：如果 `MyObject::bar()` 本身就已是 thread safe，不需要 lock，則仍必須 lock 才能使用。
    * 可以避免誤用「容易有錯誤認知的 member」，例如： `std::string::empty() const` 是 thread safe 嗎?
  
## `fon9::DummyMutex`
[fon9/DummyMutex.hpp](../fon9/DummyMutex.hpp)
* 提供 mutex 相容的介面，但沒有鎖住任何東西。

## SleepPolicy
[fon9/SleepPolicy.hpp](../fon9/SleepPolicy.hpp)
* `fon9::BusySleepPolicy`
* `fon9::YieldSleepPolicy`
* `fon9::NanoSleepPolicy<ns>`

## `fon9::SpinMutex<SleepPolicy>`
[fon9/SpinMutex.hpp](../fon9/SpinMutex.hpp)
* `using SpinBusy = SpinMutex<BusySleepPolicy>;`

## WaitPolicy
[fon9/WaitPolicy.hpp](../fon9/WaitPolicy.hpp)
* `fon9::WaitPolicy_CV`
* `fon9::WaitPolicy_Spin<MutexT>`
* `using SpinBusy = SpinMutex<BusySleepPolicy>;`

## `fon9::CountDownLatch`
[fon9/CountDownLatch.hpp](../fon9/CountDownLatch.hpp)
* 等候指定數量的倒數。

## `fon9::CyclicBarrier`
[fon9/CyclicBarrier.hpp](../fon9/CyclicBarrier.hpp)
* 當同時等候中的 Threads 到達指定數量時，則喚醒所有等候者。

## `fon9::ThreadController<ProtectedT, WaitPolicy>`
[fon9/ThreadController.hpp](../fon9/ThreadController.hpp)
* 一般 thread 執行時的資料保護、流程控制。

## `Worker<WorkContentController>`
[fon9/Worker.hpp](../fon9/Worker.hpp)
* 控制一次只會有一個 thread 執行 Worker，但是允許任意 thread 要求 Worker 做事。

## MessageQueue
[fon9/MessageQueue.hpp](../fon9/MessageQueue.hpp)
```c++
template <
   class MessageHandlerT,
   class MessageT = typename MessageHandlerT::MessageType,
   class MessageContainerT = std::deque<MessageT>,
   class WaitPolicy = WaitPolicy_CV
>
class MessageQueueService;
```

## `fon9::GetDefaultThreadPool()`
[fon9/DefaultThreadPool.hpp](../fon9/DefaultThreadPool.hpp)
* 取得 fon9 提供的一個 thread pool.
  * 一般用於不急迫, 但比較花時間的簡單工作, 例如: 寫檔、domain name 查找...
  * 程式結束時, 剩餘的工作會被拋棄!

## Timer 計時器
[fon9/Timer.hpp](../fon9/Timer.hpp)
* 由於 Timer 共用 TimerThread，所以若 OnTimer 事件的執行時間太久，
  則會影響下一個 Timer，可能會超過預計的執行時間。
### 必須先建立一個 `fon9::TimerThread` 讓 Timer 觸發的 thread。
* `fon9::TimerThread& fon9::GetDefaultTimerThread();` 可取得 fon9 預設的 TimerThread
### 允許 MyObject 在觸發前死亡
使用 `std::shared_ptr<MyObject>` + `fon9::TimerEntry_OwnerWP<MyObject, &MyObject::OnTimer>`
觸發 `MyObject::OnTimer()` 事件
```c++
class MyObject : public std::enable_shared_from_this<MyObject> {
   void OnTimer(fon9::TimerEntry* timer, fon9::TimeStamp now) {
      // ... timer 觸發後, 執行這裡.
   }
   using Timer = fon9::TimerEntry_OwnerWP<MyObject, &MyObject::OnTimer>;
public:
   void RunAfter(fon9::TimerThread& timerThread, fon9::TimeInterval ti) {
      (new Timer{timerThread, this->shared_from_this()})->RunAfter(ti);
   }
};
```
### 在 MyObject 裡面包含一個 timer 物件, 然後觸發 static member function
```c++
class MyObject {
   fon9_NON_COPY_NON_MOVE(MyObject);
   static void EmitOnTimer(fon9::TimerEntry* timer, fon9::TimeStamp now) {
      MyObject* pthis = fon9::ContainerOf(*static_cast<decltype(MyObject::Timer_)*>(timer), &MyObject::Timer_);
      // 處理 Timer 事件...
   }
   fon9::DataMemberEmitOnTimer<&MyObject::EmitOnTimer> Timer_;
public:
   MyObject(fon9::TimerThread& timerThread) : Timer_(timerThread) {
   }
   ~MyObject() {
      // 必須在 MyObject 死亡前，確定 Timer_ 已不會再觸發，否則可能會 crash!
      this->Timer_.DisposeAndWait();
   }
   void RunAfter(fon9::TimeInterval delaySecs) {
      this->Timer_.RunAfter(delaySecs);
   }
};
```
### 在 MyObject 裡面包含一個 timer 物件
* 繼承 `fon9::DataMemberTimer`
* 覆寫 `virtual void EmitOnTimer(fon9::TimeStamp now) override;`
```c++
class MyObject {
   fon9_NON_COPY_NON_MOVE(MyObject);
   class Timer : public fon9::DataMemberTimer {
      fon9_NON_COPY_NON_MOVE(Timer);
      using base = fon9::DataMemberTimer;
      virtual void EmitOnTimer(fon9::TimeStamp now) override {
         MyObject* owner = fon9::ContainerOf(*this, &MyObject::Timer_);
         // ... Timer_ 觸發後, 執行這裡.
      }
   public:
      using base::base;
   };
   Timer   Timer_;
public:
   MyObject(fon9::TimerThread& timerThread) : Timer_(timerThread) {
   }
   ~MyObject() {
      // 必須在 MyObject 死亡前，確定 Timer_ 已不會再觸發，否則可能會 crash!
      this->Timer_.DisposeAndWait();
   }
   void RunAfter(fon9::TimeInterval delaySecs) {
      this->Timer_.RunAfter(delaySecs);
   }
};
```
