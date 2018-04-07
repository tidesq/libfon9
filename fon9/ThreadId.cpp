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
   char*       pout = UIntToStrRev(pend, this->ThreadId_);
   size_t      wlen = static_cast<size_t>(pend - pout);
   this->ThreadIdStrWidth_ = static_cast<uint8_t>(wlen + 1);
   this->ThreadIdStr_[0] = ' ';
   memmove(this->ThreadIdStr_ + 1, pout, wlen);
   *(this->ThreadIdStr_ + 1 + wlen) = '\0';
}

fon9_API const ThreadId& GetThisThreadId() {
   return ThisThread_;
}

} // namespaces
