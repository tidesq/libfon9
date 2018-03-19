// \file fon9/buffer/BufferList.cpp
// \author fonwinz@gmail.com
#include "fon9/buffer/BufferList.hpp"

namespace fon9 {

fon9_API void BufferAppendTo(const BufferList& buf, std::string& str) {
   BufferAppendTo<std::string>(buf, str);
}

} // namespaces
