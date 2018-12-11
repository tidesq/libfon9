/// \file fon9/buffer/FwdBufferList.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_buffer_FwdBufferList_hpp__
#define __fon9_buffer_FwdBufferList_hpp__
#include "fon9/buffer/BufferListBuilder.hpp"
#include "fon9/buffer/FwdBuffer.hpp"

namespace fon9 {

class fon9_API DcQueue;

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

class FwdBufferListBuilder : public BufferListBuilder {
   fon9_NON_COPYABLE(FwdBufferListBuilder);
   using base = BufferListBuilder;

   static bool MergeNodeData(FwdBufferNode* last, BufferNode* node) {
      if (last == nullptr || node->GetNodeType() != BufferNodeType::Data)
         return false;
      const BufferNodeSize nodeDataSize = node->GetDataSize();
      if (last->GetRemainSize() < nodeDataSize)
         return false;
      byte*  pout = last->GetDataEnd();
      memcpy(pout, node->GetDataBegin(), nodeDataSize);
      last->SetDataEnd(pout + nodeDataSize);
      return true;
   }

public:
   using base::base;
   FwdBufferListBuilder() = delete;
   FwdBufferListBuilder(FwdBufferListBuilder&&) = default;
   FwdBufferListBuilder& operator=(FwdBufferListBuilder&&) = default;

   /// - 如果 node 或 this->back(): 非資料節點, 則直接將 node 放在最後面.
   /// - 如果 this 剩餘空間足夠, 則會將 node 的內容複製, 然後釋放 node.
   /// - 如果 this 剩餘空間不足, 則會將 node 放到最後面.
   void PushBack(BufferNode* node) {
      if (MergeNodeData(FwdBufferNode::CastFrom(this->List_.back()), node))
         FreeNode(node);
      else
         this->List_.push_back(node);
   }
   void PushBack(BufferList&& list) {
      this->List_.push_back(std::move(list));
   }
   FwdBufferNode* NewBack(size_t sz) {
      FwdBufferNode* last = FwdBufferNode::Alloc(sz + this->GetNewAllocReserved());
      this->List_.push_back(last);
      return last;
   }
   FwdBufferNode* GetBack() {
      return FwdBufferNode::CastFrom(this->List_.back());
   }

   void SetSuffixUsed(char* pout) {
      static_cast<FwdBufferNode*>(this->List_.back())->SetDataEnd(reinterpret_cast<byte*>(pout));
   }

   using base::MoveOut;
};

class fon9_API FwdBufferList : public FwdBuffer {
   fon9_NON_COPYABLE(FwdBufferList);
   FwdBufferListBuilder Builder_;
   void SetBackNodeUsed() {
      if (this->MemCurrent_)
         this->Builder_.SetSuffixUsed(this->MemCurrent_);
   }
   void ResetMemPtr(FwdBufferNode* last) {
      if (last == nullptr)
         this->MemCurrent_ = this->MemEnd_ = nullptr;
      else {
         this->MemCurrent_ = reinterpret_cast<char*>(last->GetDataEnd());
         this->MemEnd_ = this->MemCurrent_ + last->GetRemainSize();
      }
   }
   virtual char* OnFwdBufferAlloc(size_t sz) override {
      this->SetBackNodeUsed();
      this->ResetMemPtr(this->Builder_.NewBack(sz));
      return this->MemCurrent_;
   }
public:
   FwdBufferList(BufferNodeSize newAllocReserved) : FwdBuffer(), Builder_{newAllocReserved} {
   }
   FwdBufferList(BufferNodeSize newAllocReserved, BufferList&& buf) : FwdBuffer(), Builder_{newAllocReserved, std::move(buf)} {
      if (FwdBufferNode* last = static_cast<FwdBufferNode*>(this->Builder_.GetBack()))
         this->ResetMemPtr(last);
   }
   FwdBufferList(BufferNodeSize newAllocReserved, DcQueue&& extmsg);

   FwdBufferList(FwdBufferList&& rhs) : Builder_{std::move(rhs.Builder_)} {
      this->MemEnd_ = rhs.MemEnd_;
      this->MemCurrent_ = rhs.MemCurrent_;
      rhs.MemEnd_ = rhs.MemCurrent_ = nullptr;
   }
   FwdBufferList& operator=(FwdBufferList&& rhs) {
      this->Builder_ = std::move(rhs.Builder_);
      this->MemEnd_ = rhs.MemEnd_;
      this->MemCurrent_ = rhs.MemCurrent_;
      rhs.MemEnd_ = rhs.MemCurrent_ = nullptr;
      return *this;
   }

   BufferList MoveOut() {
      this->SetBackNodeUsed();
      this->MemEnd_ = this->MemCurrent_ = nullptr;
      return this->Builder_.MoveOut();
   }

   const BufferNode* cfront() {
      this->SetBackNodeUsed();
      return this->Builder_.cfront();
   }

   /// \copydoc FwdBufferListBuilder::PushBack(BufferNode* node)
   void PushBack(BufferNode* node) {
      this->SetBackNodeUsed();
      this->Builder_.PushBack(node);
      this->ResetMemPtr(FwdBufferNode::CastFrom(this->Builder_.GetBack()));
   }
   void PushBack(BufferList&& list) {
      this->SetBackNodeUsed();
      this->Builder_.PushBack(std::move(list));
      this->ResetMemPtr(FwdBufferNode::CastFrom(this->Builder_.GetBack()));
   }
};

} // namespaces
#endif//__fon9_buffer_FwdBufferList_hpp__
