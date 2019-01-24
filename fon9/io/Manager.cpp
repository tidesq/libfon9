/// \file fon9/io/Manager.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/Manager.hpp"

namespace fon9 { namespace io {

Manager::~Manager() {
}

unsigned ManagerC::IoManagerAddRef() {
   return intrusive_ptr_add_ref(static_cast<baseCounter*>(this));
}
unsigned ManagerC::IoManagerRelease() {
   return intrusive_ptr_release(static_cast<baseCounter*>(this));
}

} } // namespaces
