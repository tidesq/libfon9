/// \file fon9/ThreadId.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_ThreadId_hpp__
#define __fon9_ThreadId_hpp__
#include "fon9/StrView.hpp"

namespace fon9 {

/// \ingroup Thrs
/// this thread 的基本資料.
struct ThreadId {
   using IdType = uintptr_t;
   /// Windows=GetCurrentThreadId(), Linux=pthread_self()
   IdType   ThreadId_;
   /// ThreadId_ 轉成字串: 預設 = 從尾端算起 " nnnnn"(10進位字串), 若超過5碼, 則自動擴展.
   char     ThreadIdStr_[15];
   /// ThreadIdStr_ 不含 EOS 的長度.
   uint8_t  ThreadIdStrWidth_;

   ThreadId();

   StrView GetThreadIdStr() const {
      return StrView{this->ThreadIdStr_ + sizeof(this->ThreadIdStr_) - this->ThreadIdStrWidth_ - 1, this->ThreadIdStrWidth_};
   }
};

/// \ingroup Thrs
/// Windows 不能使用 fon9_API + thread_local, 所以必須使用 GetThisThreadId() 取得.
extern const thread_local ThreadId  ThisThread_;

/// \ingroup Thrs
/// 取得 this_thread 的 ThreadId: 可藉此取得 ThreadId_, ThreadIdStr_.
fon9_API const ThreadId& GetThisThreadId();

} // namespaces
#endif//__fon9_ThreadId_hpp__
