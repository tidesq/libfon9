// \file fon9/buffer/BufferList.cpp
// \author fonwinz@gmail.com
#include "fon9/buffer/BufferList.hpp"
#include "fon9/buffer/FwdBufferList.hpp"

namespace fon9 {

fon9_API void BufferAppendTo(const BufferList& buf, std::string& str) {
   BufferAppendTo<std::string>(buf, str);
}

fon9_API void AppendToBuffer(BufferList& dst, const void* src, size_t size) {
   FwdBufferNode* node = FwdBufferNode::CastFrom(dst.back());
   if (node == nullptr || node->GetRemainSize() < size) {
      node = FwdBufferNode::Alloc(size);
      dst.push_back(node);
   }
   byte*  ptrdst = node->GetDataEnd();
   memcpy(ptrdst, src, size);
   node->SetDataEnd(ptrdst + size);
}

} // namespaces
