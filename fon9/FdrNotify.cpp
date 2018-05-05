/// \file fon9/FdrNotify.cpp
/// \author fonwinz@gmail.com
#include "fon9/sys/Config.h"
#ifdef fon9_WINDOWS
// Window不支援!
#else//fon9_WINDOWS
#include "fon9/FdrNotify.hpp"

namespace fon9 {

#ifdef fon9_HAVE_EVENTFD
FdrNotify::Result FdrNotify::Open() {
   if (this->EventFdr_.IsReadyFD())
      return Result::kSuccess();
   int evfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
   if (evfd < 0)
      return GetSysErrC();
   this->EventFdr_.SetFD(evfd);
   if (this->EventFdr_.SetNonBlock())
      return Result::kSuccess();
   auto  errc = GetSysErrC();
   this->EventFdr_.Close();
   return errc;
}
FdrNotify::Result FdrNotify::Wakeup() {
   /// 喚醒次數加1.
   uint64_t val = 1;
   if (write(this->EventFdr_.GetFD(), &val, sizeof(val)) == sizeof(val))
      return Result::kSuccess();
   return GetSysErrC();
}
uint64_t FdrNotify::ClearWakeup() {
   uint64_t val;
   /// 讀出喚醒次數.
   if (read(this->EventFdr_.GetFD(), &val, sizeof(val)) == sizeof(val))
      return val;
   return 0;
}
#else//fon9_HAVE_EVENTFD
FdrNotify::Result FdrNotify::Open() {
   if (this->FdrRead_.IsReadyFD())
      return Result::kSuccess();
   int fds[2];
   if (::pipe(fds) != 0)
      return GetSysErrC();
   this->FdrRead_.SetFD(fds[0]);
   this->FdrWrite_.SetFD(fds[1]);
   if (this->FdrRead_.SetNonBlock() && this->FdrWrite_.SetNonBlock())
      return Result::kSuccess();
   auto eno = GetSysErrC();
   this->FdrRead_.Close();
   this->FdrWrite_.Close();
   return eno;
}
FdrNotify::Result FdrNotify::Wakeup() {
   uint8_t val = 1;
   if (write(this->FdrWrite_.GetFD(), &val, sizeof(val)) == sizeof(val))
      return Result::kSuccess();
   return GetSysErrC();
}
uint64_t FdrNotify::ClearWakeup() {
   uint8_t val[1024];
   ssize_t rdsz = read(this->FdrRead_.GetFD(), val, sizeof(val));
   if (rdsz > 0)
      return static_cast<uint64_t>(rdsz);
   return 0;
}
#endif//fon9_HAVE_EVENTFD
} // namespaces
#endif//fon9_WINDOWS
