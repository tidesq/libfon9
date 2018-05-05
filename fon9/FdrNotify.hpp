/// \file fon9/FdrNotify.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_FdrNotify_hpp__
#define __fon9_FdrNotify_hpp__
#include "fon9/Fdr.hpp"
#include "fon9/Outcome.hpp"

#ifdef __linux__
#define fon9_HAVE_EVENTFD
#include <sys/eventfd.h>
#endif

namespace fon9 {

/// \ingroup Others
/// - Linux:   使用 eventfd() 實現.
/// - 非linux: 使用 pipe() 實現.
class fon9_API FdrNotify {
   fon9_NON_COPY_NON_MOVE(FdrNotify);
#ifdef fon9_HAVE_EVENTFD
   FdrAuto  EventFdr_;
#else
   FdrAuto  FdrRead_;
   FdrAuto  FdrWrite_;
#endif
public:
   using Result = Result3;

   FdrNotify() {
      this->Open();
   }
   /// 開啟 fd, 若建構時已開啟成功, 不會重新開啟.
   Result Open();
   /// 喚醒讀取端.
   Result Wakeup();
   /// 取得讀取端的 fd.
   Fdr::fdr_t GetReadFD() const {
      #ifdef fon9_HAVE_EVENTFD
         return EventFdr_.GetFD();
      #else
         return FdrRead_.GetFD();
      #endif
   }
   /// 傳回 Wakeup() 呼叫的次數, 並清除計數器.
   uint64_t ClearWakeup();
};

} // namespaces
#endif//__fon9_FdrNotify_hpp__
