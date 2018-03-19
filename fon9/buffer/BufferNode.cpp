// \file fon9/buffer/BufferNode.cpp
// \author fonwinz@gmail.com
#include "fon9/buffer/BufferNode.hpp"

namespace fon9 {

BufferNodeVirtual::~BufferNodeVirtual() {
}

fon9_API void FreeNode(BufferNode* node) {
   switch (node->GetNodeType()) {
   case BufferNodeType::Data:
      break;
   case BufferNodeType::Virtual:
      BufferNodeVirtual* vnode = static_cast<BufferNodeVirtual*>(node);
      vnode->~BufferNodeVirtual();
      // 有 vtbl, 所以 free 的對象必須是: vnode (而不是 node); (void*)node 不一定與 (void*)vnode 相等.
      MemBlock::FreeBlock(vnode, vnode->BlockSize_);
      return;
   }
   MemBlock::FreeBlock(node, node->BlockSize_);
}

} // namespaces
