/// \file fon9/io/win/IocpSocketClient.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/win/IocpSocketClient.hpp"

namespace fon9 { namespace io {

unsigned IocpSocketClientImpl::IocpSocketAddRef() {
   return intrusive_ptr_add_ref(this);
}
unsigned IocpSocketClientImpl::IocpSocketReleaseRef() {
   return intrusive_ptr_release(this);
}

} } // namespaces
