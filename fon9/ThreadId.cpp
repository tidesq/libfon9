// \file fon9/ThreadId.cpp
// \author fonwinz@gmail.com
#include "fon9/ThreadId.hpp"
#include "fon9/ToStr.hpp"

#ifndef fon9_WINDOWS
#include <sys/syscall.h>//For SYS_xxx definitions
inline static fon9::ThreadId::IdType GetCurrentThreadId() {
   // return static_cast<fon9::ThreadId::IdType>(pthread_self());//pthread_self()用的是一個內部指標, 與 OS 的 thread-id 無關, 所以並不適合.
   return static_cast<fon9::ThreadId::IdType>(::syscall(SYS_gettid));
   // return static_cast<fon9::ThreadId::IdType>(syscall(__NR_gettid));
}
#endif

namespace fon9 {

const thread_local ThreadId  ThisThread_;

ThreadId::ThreadId() : ThreadId_{::GetCurrentThreadId()} {
   char* const pend = this->ThreadIdStr_ + sizeof(this->ThreadIdStr_);
   char* pout = pend;
   *--pout = 0;
   pout = UIntToStrRev(pout, this->ThreadId_);
   uint8_t w = static_cast<uint8_t>(pend - pout - 1);
   while(w++ < 5)
      *--pout = ' ';
   *--pout = ' ';
   this->ThreadIdStrWidth_ = w;
}

fon9_API const ThreadId& GetThisThreadId() {
   return ThisThread_;
}

} // namespaces
