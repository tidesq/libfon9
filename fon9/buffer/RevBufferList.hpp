/// \file fon9/buffer/RevBufferList.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_buffer_RevBufferList_hpp__
#define __fon9_buffer_RevBufferList_hpp__
#include "fon9/buffer/BufferListBuilder.hpp"
#include "fon9/buffer/RevBuffer.hpp"

namespace fon9 {

class fon9_API DcQueue;

/// \ingroup Buffer
/// 一個由 **後往前** 填入資料的緩衝區節點.
/// - 從 GetDataBegin()-1 開始往前填入資料.
/// - 最多只能填到 GetMemBegin() 的位置.
/// - 所以尚可填入的資料量 = GetDataBegin() - GetMemBegin();
class RevBufferNode : public BufferNode {
   fon9_NON_COPY_NON_MOVE(RevBufferNode);
   ~RevBufferNode() = delete;
   RevBufferNode(BufferNodeSize blockSize) : BufferNode(blockSize, blockSize) {
   }
   friend RevBufferNode* BufferNode::Alloc<RevBufferNode>(size_t);
   // RevBufferNode, 應把新資料放在 GetDataBegin() 之前,
   // 不會有 GetDataEnd() 的需求, 為了避免誤用, 所以移入 private.
   using BufferNode::GetDataEnd;
public:
   static RevBufferNode* Alloc(size_t size) {
      return BufferNode::Alloc<RevBufferNode>(size);
   }
   void SetPrefixUsed(byte* pout) {
      assert(this->GetMemBegin() <= pout && pout <= this->GetDataBegin());
      this->DataBeginOffset_ = static_cast<BufferNodeSize>(pout - reinterpret_cast<byte*>(this));
   }
   BufferNodeSize GetRemainSize() const {
      return static_cast<BufferNodeSize>(this->GetDataBegin() - this->GetMemBegin());
   }
   static RevBufferNode* CastFrom(BufferNode* node) {
      return(node && node->GetNodeType() == BufferNodeType::Data) ? static_cast<RevBufferNode*>(node) : nullptr;
   }
};

class RevBufferListBuilder : public BufferListBuilder {
   fon9_NON_COPYABLE(RevBufferListBuilder);
   using base = BufferListBuilder;

   static bool MergeNodeData(BufferNode* front, RevBufferNode* next) {
      if (next == nullptr || front->GetNodeType() != BufferNodeType::Data)
         return false;
      const BufferNodeSize frontDataSize = front->GetDataSize();
      if (next->GetRemainSize() < frontDataSize)
         return false;
      byte*  pout = next->GetDataBegin() - frontDataSize;
      memcpy(pout, front->GetDataBegin(), frontDataSize);
      next->SetPrefixUsed(pout);
      return true;
   }

public:
   using base::base;
   RevBufferListBuilder() = delete;
   RevBufferListBuilder(RevBufferListBuilder&&) = default;
   RevBufferListBuilder& operator=(RevBufferListBuilder&&) = default;

   /// - 如果 node 或 this->cfront(): 非資料節點, 則直接將 node 放在最前面.
   /// - 如果 this 剩餘空間足夠, 則會將 node 的內容複製, 然後釋放 node.
   /// - 如果 this 剩餘空間不足, 則會將 node 放到最前面.
   void PushFront(BufferNode* node) {
      if (MergeNodeData(node, RevBufferNode::CastFrom(this->List_.front())))
         FreeNode(node);
      else
         this->List_.push_front(node);
   }
   RevBufferNode* NewFront(size_t sz) {
      RevBufferNode* front = RevBufferNode::Alloc(sz + this->GetNewAllocReserved());
      this->List_.push_front(front);
      return front;
   }
   RevBufferNode* GetFront() {
      return RevBufferNode::CastFrom(this->List_.front());
   }

   void SetPrefixUsed(char* pout) {
      static_cast<RevBufferNode*>(this->List_.front())->SetPrefixUsed(reinterpret_cast<byte*>(pout));
   }

   BufferList MoveOut() {
      if (fon9_UNLIKELY(this->List_.size() > 1)) {
         BufferNode* front = this->List_.front();
         if (MergeNodeData(front, RevBufferNode::CastFrom(front->GetNext())))
            FreeNode(this->List_.pop_front());
      }
      return base::MoveOut();
   }
};

class fon9_API RevBufferList : public RevBuffer {
   fon9_NON_COPYABLE(RevBufferList);
   RevBufferListBuilder Builder_;
   void SetFrontNodeUsed() {
      if (this->MemCurrent_)
         this->Builder_.SetPrefixUsed(this->MemCurrent_);
   }
   void ResetMemPtr(RevBufferNode* front) {
      if (front == nullptr)
         this->MemCurrent_ = this->MemBegin_ = nullptr;
      else {
         this->MemCurrent_ = reinterpret_cast<char*>(front->GetDataBegin());
         this->MemBegin_ = this->MemCurrent_ - front->GetRemainSize();
      }
   }
   virtual char* OnRevBufferAlloc(size_t sz) override {
      this->SetFrontNodeUsed();
      this->ResetMemPtr(this->Builder_.NewFront(sz));
      return this->MemCurrent_;
   }
public:
   RevBufferList(BufferNodeSize newAllocReserved) : RevBuffer(), Builder_{newAllocReserved} {
   }
   RevBufferList(BufferNodeSize newAllocReserved, BufferList&& buf) : RevBuffer(), Builder_{newAllocReserved, std::move(buf)} {
      if (RevBufferNode* front = static_cast<RevBufferNode*>(this->Builder_.GetFront()))
         this->ResetMemPtr(front);
   }
   RevBufferList(BufferNodeSize newAllocReserved, DcQueue&& extmsg);

   RevBufferList(RevBufferList&& rhs) : Builder_{std::move(rhs.Builder_)} {
      this->MemBegin_ = rhs.MemBegin_;
      this->MemCurrent_ = rhs.MemCurrent_;
      rhs.MemBegin_ = rhs.MemCurrent_ = nullptr;
   }
   RevBufferList& operator=(RevBufferList&& rhs) {
      this->Builder_ = std::move(rhs.Builder_);
      this->MemBegin_ = rhs.MemBegin_;
      this->MemCurrent_ = rhs.MemCurrent_;
      rhs.MemBegin_ = rhs.MemCurrent_ = nullptr;
      return *this;
   }

   BufferList MoveOut() {
      this->SetFrontNodeUsed();
      this->MemBegin_ = this->MemCurrent_ = nullptr;
      return this->Builder_.MoveOut();
   }

   const BufferNode* cfront() {
      this->SetFrontNodeUsed();
      return this->Builder_.cfront();
   }

   /// \copydoc RevBufferListBuilder::PushFront(BufferNode* node)
   void PushFront(BufferNode* node) {
      this->SetFrontNodeUsed();
      this->Builder_.PushFront(node);
      this->ResetMemPtr(RevBufferNode::CastFrom(this->Builder_.GetFront()));
   }
};

} // namespaces
#endif//__fon9_buffer_RevBufferList_hpp__
