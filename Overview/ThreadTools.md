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
