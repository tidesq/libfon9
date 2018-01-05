/// \file fon9/DummyMutex.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_DummyMutex_hpp__
#define __fon9_DummyMutex_hpp__
#include "fon9/sys/Config.hpp"

namespace fon9 {

class DummyMutex {
   fon9_NON_COPY_NON_MOVE(DummyMutex);
public:
   DummyMutex() = default;
   void lock() {}
   void unlock() {}
};

} // namespaces
#endif//__fon9_DummyMutex_hpp__
