// \file fon9/buffer/DcQueueList.cpp
// \author fonwinz@gmail.com
#include "fon9/buffer/DcQueueList.hpp"

namespace fon9 {

void DcQueueList::FrontToCurrBlock() {
   BufferNode* front = this->BlockList_.front();
   if (this->MemCurrent_ != nullptr) {
      assert(front && front->GetDataBegin() <= this->MemCurrent_ && this->MemCurrent_ < front->GetDataEnd());
      return;
   }
   while (front) {
      if (front->GetDataSize() > 0) {
         this->ResetCurrBlock(front->GetDataBegin(), front->GetDataEnd());
         return;
      }
      this->NodeConsumed(this->BlockList_.pop_front());
      front = this->BlockList_.front();
   }
}
BufferList DcQueueList::MoveOut() {
   BufferNode* front = this->BlockList_.front();
   if (front == nullptr) {
      assert(this->MemCurrent_ == nullptr);
      return BufferList{};
   }
   assert(this->MemCurrent_ != nullptr);
   if(BufferNodeSize szUsed = static_cast<BufferNodeSize>(this->MemCurrent_ - front->GetDataBegin()))
      front->MoveDataBeginOffset(szUsed);
   this->ClearCurrBlock();
   return std::move(this->BlockList_);
}
bool DcQueueList::DcQueuePeekMore(byte* tmpbuf, size_t sz) {
   if (const BufferNode* node = this->BlockList_.front()) {
      while ((node = node->GetNext()) != nullptr) {
         if (const size_t nodesz = node->GetDataSize()) {
            const size_t peeksz = (sz < nodesz ? sz : nodesz);
            memcpy(tmpbuf, node->GetDataBegin(), peeksz);
            if ((sz -= peeksz) <= 0)
               return true;
            tmpbuf += peeksz;
         }
      }
   }
   return false;
}
bool DcQueueList::DcQueueHasMore(size_t sz) const {
   if (const BufferNode* node = this->BlockList_.front()) {
      size_t nodesz = 0;
      while ((node = node->GetNext()) != nullptr) {
         if ((nodesz += node->GetDataSize()) >= sz)
            return true;
      }
   }
   return false;
}

void DcQueueList::DcQueueRemoveMore(size_t sz) {
   // 先釋放 curr block.
   if (BufferNode* front = this->BlockList_.pop_front())
      this->NodeConsumed(front);
   else {
      assert(sz == 0);
      return;
   }
   // 從後續的 node 移除資料, 並在返回前設定 curr block.
   while (BufferNode* node = this->BlockList_.front()) {
      if (size_t nodesz = node->GetDataSize()) {
         if (sz < nodesz) {
            this->ResetCurrBlock(node->GetDataBegin() + sz, node->GetDataEnd());
            return;
         }
         sz -= nodesz;
      }
      this->NodeConsumed(this->BlockList_.pop_front());
   }
   assert(sz == 0 && this->BlockList_.empty());
   this->ClearCurrBlock();
}
size_t DcQueueList::DcQueueReadMore(byte* buf, size_t sz) {
   // 先釋放 curr block.
   this->NodeConsumed(this->BlockList_.pop_front());
   // 從後續的 node 取出資料, 並在返回前設定 curr block.
   size_t rdsz = 0;
   while (BufferNode* node = this->BlockList_.front()) {
      if (size_t nodesz = node->GetDataSize()) {
         byte* beg = node->GetDataBegin();
         if (sz < nodesz) {
            memcpy(buf + rdsz, beg, sz);
            this->ResetCurrBlock(beg + sz, node->GetDataEnd());
            return rdsz + sz;
         }
         memcpy(buf + rdsz, beg, nodesz);
         rdsz += nodesz;
         sz -= nodesz;
      }
      this->NodeConsumed(this->BlockList_.pop_front());
   }
   assert(this->BlockList_.empty());
   this->ClearCurrBlock();
   return rdsz;
}

void DcQueueList::ConsumeErr(const ErrC& errc) {
   BufferNode* node = this->BlockList_.pop_front();
   if (node == nullptr)
      return;
   this->NodeConsumed(node, errc, static_cast<BufferNodeSize>(this->MemCurrent_ - node->GetDataBegin()));
   while ((node = this->BlockList_.pop_front()) != nullptr) {
      if (BufferNodeVirtual* vnode = BufferNodeVirtual::CastFrom(node))
         vnode->OnBufferConsumedErr(errc);
      this->NodeConsumed(node, errc, node->GetDataSize());
   }
   assert(this->BlockList_.empty());
   this->ClearCurrBlock();
}

} // namespaces
