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
inline void JoinThreads(ThreadContainer& thrs) {
   for (std::thread& thr : thrs)
      JoinThread(thr);
}

inline void JoinOrDetach(std::thread& thr) {
   if (thr.get_id() == std::this_thread::get_id())
      thr.detach();
   else
   if (thr.joinable())
      thr.join();
}

template <class ThreadContainer>
inline void JoinOrDetach(ThreadContainer& thrs) {
   for (std::thread& thr : thrs)
      JoinOrDetach(thr);
}

} // namespace fon9
#endif//__fon9_ThreadTools_hpp__
