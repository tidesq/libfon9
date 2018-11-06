/// \file fon9/io/FdrSocketClient.cpp
/// \author fonwinz@gmail.com
#include "fon9/sys/Config.h"
#ifdef fon9_POSIX
#include "fon9/io/FdrSocketClient.hpp"

namespace fon9 { namespace io {

void FdrSocketClientImpl::OnFdrEvent_AddRef() {
   intrusive_ptr_add_ref(static_cast<baseCounter*>(this));
}
void FdrSocketClientImpl::OnFdrEvent_ReleaseRef() {
   intrusive_ptr_release(static_cast<baseCounter*>(this));
}
void FdrSocketClientImpl::OpImpl_Close() {
   this->State_ = State::Closing;
   this->EnabledEvents_.store(0, std::memory_order_relaxed);
   this->RemoveFdrEvent();
   ::shutdown(this->GetFD(), SHUT_WR);
}

} } // namespaces
#endif
