// \file fon9/seed/fon9Co.cpp
//
// fon9 console
//
// \author fonwinz@gmail.com
#include "fon9/framework/Framework.hpp"

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

int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
   //_CrtSetBreakAlloc(176);
#endif
   fon9::Framework   fon9sys;
   fon9sys.Initialize(argc, argv);
   fon9sys.Dispose();
}
