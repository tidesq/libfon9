// \file fon9/sys/OnWindowsMainExit.cpp
// \author fonwinz@gmail.com
#include "fon9/sys/OnWindowsMainExit.hpp"
#include "fon9/Utility.hpp"
#include <atomic>

namespace fon9 { namespace sys {

#if defined(_MSC_VER) && defined(_DLL)
/// atexit() 只會註冊到 dll 的結束執行表(會在.exe.main()結束 & thread 強制結束後)執行.
/// 所以要用 _crt_atexit(): 將 func 註冊到 exe 的結束執行表.
/// 副作用: 如果 exe 沒有直接使用到 libfon9.dll, libfon9.dll 仍會延遲到 exe 結束後才卸載.
extern "C" void _crt_atexit(void (__cdecl* func)(void));

static std::atomic<unsigned>     OnWindowsMainExitHandleCount_{};
static OnWindowsMainExitHandle*  OnWindowsMainExitHandles_[16];
OnWindowsMainExitHandle::OnWindowsMainExitHandle() {
   struct Aux {
      static void __cdecl OnAtexitCallback() {
         const unsigned count = OnWindowsMainExitHandleCount_;
         unsigned L = count;
         OnWindowsMainExitHandle* hdr;
         while (L > 0)
            if ((hdr = OnWindowsMainExitHandles_[--L]) != nullptr)
               hdr->OnWindowsMainExit_Notify();
         L = count;
         while (L > 0)
            if ((hdr = OnWindowsMainExitHandles_[--L]) != nullptr)
               hdr->OnWindowsMainExit_ThreadJoin();
      }
      static HMODULE GetCurrentModule() {
         HMODULE hModule = NULL;
         GetModuleHandleEx(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            (LPCTSTR)GetCurrentModule,
            &hModule);
         return hModule;
      }
      Aux() {
         _crt_atexit(&OnAtexitCallback);
         char  fname[1024 * 4];
         GetModuleFileName(GetCurrentModule(), fname, sizeof(fname));
         LoadLibrary(fname);
      }
   };
   static Aux aux;
   assert(OnWindowsMainExitHandleCount_ < numofele(OnWindowsMainExitHandles_)
          && "numofele(OnWindowsMainExitHandles_) too small");
   unsigned cidx = OnWindowsMainExitHandleCount_;
   if(cidx < numofele(OnWindowsMainExitHandles_))
      if((cidx = OnWindowsMainExitHandleCount_++) < numofele(OnWindowsMainExitHandles_))
         OnWindowsMainExitHandles_[cidx] = this;
}
OnWindowsMainExitHandle::~OnWindowsMainExitHandle() {
   unsigned L = OnWindowsMainExitHandleCount_;
   while (L > 0)
      if (OnWindowsMainExitHandles_[--L] == this)
         OnWindowsMainExitHandles_[L] = nullptr;
}
#endif

} } // namespaces
