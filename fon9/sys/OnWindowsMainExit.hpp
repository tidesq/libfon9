/// \file fon9/sys/OnWindowsMainExit.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_sys_OnWindowsMainExit_hpp__
#define __fon9_sys_OnWindowsMainExit_hpp__

namespace fon9 { namespace sys {

#if defined(_MSC_VER) && defined(_DLL)
   #define fon9_HAVE_OnWindowsMainExitHandle
   /// 如果 libfon9 是 Windows DLL, 則需要:
   /// - 在 exe.main() 結束後 & libfon9.dll 卸載前: 主動關閉 fon9 內部的 thread(例: MemBlock::Center).
   /// - 否則在 libfon9.dll 卸載前, thread 會被強制結束! 無法安全的清理 thread 裡面的資源!
   struct OnWindowsMainExitHandle {
      OnWindowsMainExitHandle();
      virtual void OnWindowsMainExit_Notify() = 0;
      virtual void OnWindowsMainExit_ThreadJoin() = 0;
   protected:
      virtual ~OnWindowsMainExitHandle();
   };
#else
   struct OnWindowsMainExitHandle {
   };
#endif

} } // namespaces

#endif//__fon9_sys_OnWindowsMainExit_hpp__
