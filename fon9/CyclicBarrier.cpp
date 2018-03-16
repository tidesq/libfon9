/// \file fon9/CyclicBarrier.cpp
/// \author fonwinz@gmail.com
#include "fon9/CyclicBarrier.hpp"

namespace fon9 {

void CyclicBarrier::Wait() {
   Locker   locker(this->Mutex_);
   if (++this->WaittingCount_ < this->ExpectedCount_) {
      this->Latch_.Wait(locker);
   } else {
      this->WaittingCount_ = 0;
      this->Latch_.NotifyAll(locker);
   }
}

} // namespace
