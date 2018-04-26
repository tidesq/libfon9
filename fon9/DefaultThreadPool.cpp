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
         // 目前只有 log file & DN resolve(io/SocketAddressDN.cpp) 用到, 以後可能有...
         uint32_t threadCount = 2;

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
