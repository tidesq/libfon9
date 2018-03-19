/// \file fon9/CountDownLatch.cpp
/// \author fonwinz@gmail.com
#include "fon9/CountDownLatch.hpp"

namespace fon9 {

void CountDownLatch::Wait() {
   Locker   locker(this->Mutex_);
   while (this->Counter_ > 0)
      this->Waiter_.Wait(locker);
}
void CountDownLatch::CountDown() {
   Locker   locker(this->Mutex_);
   if (--this->Counter_ > 0)
      return;
   this->Counter_ = 0;
   this->Waiter_.NotifyAll(locker);
}
int CountDownLatch::ForceWakeUp() {
   Locker   locker(this->Mutex_);
   int bfcount = this->Counter_;
   this->Counter_ = 0;
   this->Waiter_.NotifyAll(locker);
   return bfcount;
}
int CountDownLatch::AddCounter(unsigned count) {
   Locker   locker(this->Mutex_);
   int bfcount = this->Counter_;
   this->Counter_ += count;
   return bfcount;
}

} // namespace
