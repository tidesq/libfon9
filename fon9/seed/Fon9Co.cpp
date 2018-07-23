// \file fon9/seed/fon9Co.cpp
//
// fon9 console
//
// \author fonwinz@gmail.com
#include "fon9/seed/MaTree.hpp"
#include "fon9/auth/AuthMgr.hpp"

#if !defined(fon9_WINDOWS)
#include <sys/mman.h> // mlockall()
#endif

#ifdef __GNUC__
#include <mcheck.h>
static void prepare_mtrace(void) __attribute__((constructor));
static void prepare_mtrace(void) { mtrace(); }

// 在 int main() 的 cpp 裡面: 加上這個永遠不會被呼叫的 fn,
// 就可解決 g++: "Enable multithreading to use std::thread: Operation not permitted" 的問題了!
void FixBug_use_std_thread(pthread_t* thread, void *(*start_routine) (void *)) {
   pthread_create(thread, NULL, start_routine, NULL);
}
#endif

//--------------------------------------------------------------------------//

//--------------------------------------------------------------------------//

int main(int argc, char** args) {
#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
   //_CrtSetBreakAlloc(176);
#endif

   (void)argc; (void)args;

   fon9::seed::MaTreeSP MaTree_{new fon9::seed::MaTree{"MaRoot"}};
   if (auto authmgr = fon9::auth::AuthMgr::Plant(MaTree_)) {
   }
   MaTree_->ClearSeeds();
}
