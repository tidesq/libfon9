libfon9 基礎建設程式庫 - 雜項
=============================

## 基本說明
較難歸類的小工具，放在此處說明

---------------------------------------

## 一般工具
### 返回值可包含錯誤碼: `Outcome<>`
* [`fon9/Outcome.hpp`](../fon9/Outcome.hpp)
* `Outcome<Result, Error=ErrC>`

### 錯誤碼 `ErrC`
* [`fon9/ErrC.hpp`](../fon9/ErrC.hpp)
* `using ErrC = std::error_condition;`
* Windows: `ErrC GetSysErrC(DWORD eno = ::GetLastError());`
* UNIX: `ErrC GetSysErrC(int eno = errno);`
* `void RevPrint(RevBuffer& rbuf, ErrC errc);`

### StaticPtr
* [`fon9/StaticPtr.hpp`](../fon9/StaticPtr.hpp)
* 取代 `static std::unique_ptr<T> ptr;` 或 `static thread_local std::unique_ptr<T> ptr;`
* 因為在 ptr 死亡後，可能還會用到 ptr。
* 增加 `ptr.IsDisposed()` 判斷 ptr 本身(不是所指物件)，是否已經死亡。

### intrusive_ptr<>、intrusive_ref_counter<>
* [`fon9/intrusive_ptr.hpp`](../fon9/intrusive_ptr.hpp)
  * [參考 boost 的文件](http://www.boost.org/doc/libs/1_60_0/libs/smart_ptr/intrusive_ptr.html)
  * fon9 的額外調整: 在 `intrusive_ptr_release()` 時, 如果需要刪除, 則會呼叫 `intrusive_ptr_deleter()`;
    * 如此一來就可以自訂某型別在 `intrusive_ptr<>` 的刪除行為，預設直接呼叫 delete
    * 例:
      ```c++
      class MyClass : public fon9::intrusive_ref_counter<MyClass> {
         virtual void OnBeforeDestroy() const {
         }
         // 自訂 intrusive_ptr<> 刪除 MyClass 的方法.
         inline friend void intrusive_ptr_deleter(const MyClass* p) {
            p->OnBeforeDestroy();
            delete p;
         }
      };
      ```
* [`fon9/intrusive_ref_counter.hpp`](../fon9/intrusive_ref_counter.hpp)
  * [參考 boost 的文件](http://www.boost.org/doc/libs/1_60_0/libs/smart_ptr/intrusive_ref_counter.html)
  * 提供類似 `std::make_shared()` 但使用 `intrusive_ref_counter` 機制的: `ObjHolder<ObjT>`, `MakeObjHolder(...)`

### HostId
* [`fon9/HostId.hpp`](../fon9/HostId.hpp)
* `extern fon9_API HostId  LocalHostId_;`

---------------------------------------

## 設定檔載入工具
* [`fon9/ConfigLoader.hpp`](../fon9/ConfigLoader.hpp)

## 載入模組
### 定義執行入口
* [`fon9/seed/Plugins.hpp`](../fon9/seed/Plugins.hpp)
* `fon9::seed::PluginsDesc`
* `fon9::seed::PluginsPark`
* 範例:
  * [TcpClient factory](../fon9/framework/IoFactoryTcpClient.cpp)
  * [TcpServer factory](../fon9/framework/IoFactoryTcpServer.cpp)
  * [Http session](../fon9/web/HttpPlugins.cpp)
### 載入管理員
* [`fon9/seed/PluginsMgr.hpp`](../fon9/seed/PluginsMgr.hpp)
