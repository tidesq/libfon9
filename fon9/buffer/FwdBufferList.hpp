/// \file fon9/buffer/FwdBufferList.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_buffer_FwdBufferList_hpp__
#define __fon9_buffer_FwdBufferList_hpp__
#include "fon9/buffer/BufferListBuilder.hpp"

namespace fon9 {

/// \ingroup Buffer
/// 一個由 **前往後** 填入資料的緩衝區節點.
/// - 從 GetDataEnd() 開始往後填入資料.
/// - 最多只能填到 GetMemEnd()-1 的位置.
/// - 所以尚可填入的資料量 = GetMemEnd() - GetDataEnd();
class FwdBufferNode : public BufferNode {
   fon9_NON_COPY_NON_MOVE(FwdBufferNode);
   ~FwdBufferNode() = delete;
   FwdBufferNode(BufferNodeSize blockSize) : BufferNode(blockSize, GetMemBeginOffset()) {
   }
   friend FwdBufferNode* BufferNode::Alloc<FwdBufferNode>(size_t);
   // FwdBufferNode, 應把新資料放在 GetDataEnd() 之後,
   // 不會有 GetDataBegin() 的需求, 為了避免誤用, 所以移入 private.
   using BufferNode::GetDataBegin;
public:
   static FwdBufferNode* Alloc(size_t size) {
      return BufferNode::Alloc<FwdBufferNode>(size);
   }
   /// 分配一個前端保留一小塊(sizeReserved)記憶體的 FwdBufferNode.
   /// 前方保留的小塊記憶體: 可以將 node 轉成 RevBufferNode 之後使用.
   static FwdBufferNode* AllocReserveFront(size_t size, size_t sizeReserved) {
      FwdBufferNode* node = Alloc(size);
      node->DataEndOffset_ = node->DataBeginOffset_ += static_cast<BufferNodeSize>(sizeReserved);
      return node;
   }
   void SetDataEnd(byte* pend) {
      assert(this->GetDataEnd() <= pend && pend <= this->GetMemEnd());
      this->DataEndOffset_ = static_cast<BufferNodeSize>(pend - reinterpret_cast<byte*>(this));
   }
   BufferNodeSize GetRemainSize() const {
      return static_cast<BufferNodeSize>(this->GetMemEnd() - this->GetDataEnd());
   }
   static FwdBufferNode* CastFrom(BufferNode* node) {
      return (node && node->GetNodeType() == BufferNodeType::Data) ? static_cast<FwdBufferNode*>(node) : nullptr;
   }
};

} // namespaces
#endif//__fon9_buffer_FwdBufferList_hpp__
