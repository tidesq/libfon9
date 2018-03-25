/// \file fon9/ThreadTools.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_ThreadTools_hpp__
#define __fon9_ThreadTools_hpp__
#include <thread>

namespace fon9 {

inline void JoinThread(std::thread& thr) {
   if (thr.joinable())
      thr.join();
}

template <class ThreadContainer>
void JoinThreads(ThreadContainer& thrs) {
   for (std::thread& thr : thrs)
      JoinThread(thr);
}

} // namespace fon9
#endif//__fon9_ThreadTools_hpp__
