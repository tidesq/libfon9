/// \file fon9/io/RecvBuffer.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/RecvBuffer.hpp"

namespace fon9 { namespace io {

void RecvBuffer::Clear() {
   this->State_ = RecvBufferState::NotInUse;
   this->NodeBack_ = nullptr;
   if (this->NodeReserve_) {
      FreeNode(this->NodeReserve_);
      this->NodeReserve_ = nullptr;
   }
   this->Queue_.MoveOut();
}

FwdBufferNode* RecvBuffer::AllocReserve(size_t expectSize) {
   if (expectSize > 0) {
      if (this->NodeReserve_) {
         assert(this->NodeReserve_->GetDataSize() == 0);
         if (this->NodeReserve_->GetRemainSize() >= expectSize)
            return this->NodeReserve_;
         FreeNode(this->NodeReserve_);
      }
      this->NodeReserve_ = FwdBufferNode::Alloc(expectSize);
   }
   return this->NodeReserve_;
}

//-------------------------------------------------------------------//

static size_t SetNodeDataReceived(FwdBufferNode* node, const size_t rxsz) {
   size_t nsz = node->GetRemainSize();
   assert(nsz > 0);
   if (nsz > rxsz)
      nsz = rxsz;
   node->SetDataEnd(node->GetDataEnd() + nsz);
   return rxsz - nsz;
}
DcQueueList& RecvBuffer::SetDataReceived(size_t rxsz) {
   assert(this->State_ == RecvBufferState::Receiving);
   this->State_ = RecvBufferState::InvokingEvent;
   if (fon9_LIKELY(rxsz > 0)) {
      if (this->NodeBack_) {
         rxsz = SetNodeDataReceived(this->NodeBack_, rxsz);
         this->Queue_.BackExpanded(this->NodeBack_);
         this->NodeBack_ = nullptr;
      }
      if (rxsz > 0 && this->NodeReserve_) {
         rxsz = SetNodeDataReceived(this->NodeReserve_, rxsz);
         this->Queue_.push_back(this->NodeReserve_);
         this->NodeReserve_ = nullptr;
      }
      assert(rxsz == 0); // 收進來的資料量, 必定不會超過 GetRecvBlockVector() 所取出的區塊大小.
   }
   return this->Queue_;
}

} } // namespace
