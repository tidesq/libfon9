// \file fon9/DefaultThreadPool.cpp
// \author fonwinz@gmail.com
#include "fon9/DefaultThreadPool.hpp"
#include "fon9/sys/OnWindowsMainExit.hpp"

namespace fon9 {

struct DefaultThreadTaskHandler {
   using MessageType = DefaultThreadTask;
   DefaultThreadTaskHandler(DefaultThreadPool&) {
      if (gWaitLogSystemReady)
         gWaitLogSystemReady();
   }
   void OnMessage(DefaultThreadTask& task) {
      task();
   }
   void OnThreadEnd(const std::string& threadName) {
      (void)threadName;
   }
};

fon9_API DefaultThreadPool& GetDefaultThreadPool() {
   struct DefaultThreadPoolImpl : public DefaultThreadPool, sys::OnWindowsMainExitHandle {
      fon9_NON_COPY_NON_MOVE(DefaultThreadPoolImpl);
      DefaultThreadPoolImpl() {
         // TODO: 從 getenv()、argv 取得參數?
         // 目前有用到的地方:
         //  - log file(FileAppender)
         //  - DN resolve(io/SocketAddressDN.cpp)
         //  - Device 執行 OpQueue_
         //  - InnDbf
         //  - ...
         // 這裡的 threadCount 應該考慮以上不同工作種類的數量, 與 CPU 核心數無關.
         // 因為以上的工作都是: 低 CPU 用量, 且 IO blocking.
         uint32_t threadCount = 4;

         this->StartThread(threadCount, "fon9.DefaultThreadPool");
      }
      void OnWindowsMainExit_Notify() {
         this->NotifyForEndNow();
      }
      void OnWindowsMainExit_ThreadJoin() {
         this->WaitForEndNow();
      }
   };
   static DefaultThreadPoolImpl ThreadPool_;
   return ThreadPool_;
}

} // namespaces
