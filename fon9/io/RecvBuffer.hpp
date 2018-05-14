/// \file fon9/io/BufferIo.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_RecvBuffer_hpp__
#define __fon9_io_RecvBuffer_hpp__
#include "fon9/io/IoBase.hpp"
#include "fon9/buffer/DcQueueList.hpp"
#include "fon9/buffer/FwdBufferList.hpp"

namespace fon9 { namespace io {

enum class RecvBufferState {
   /// 沒有在收資料.
   NotInUse,
   /// 資料接收中.
   Receiving,
   /// 正在觸發「收到資料」事件.
   InvokingEvent,
   /// 移到另一個 thread 觸發「收到資料」事件.
   WaitingEventInvoke,
};

fon9_WARN_DISABLE_PADDING;
/// \ingroup io
/// 1. 根據需要的資料量分配緩衝區: GetRecvBlockVector();
/// 2. 根據 GetRecvBlockVector() 取得的緩衝區開始收資料.
/// 3. 收到資料後(可能在另一個 thread), 設定收到的資料量: SetDataReceived();
/// 4. 根據 SetDataReceived() 取得: 已收到可供消費的資料.
/// 5. 消費資料之後, 回到步驟 1. 繼續接收資料.
class fon9_API RecvBuffer {
   fon9_NON_COPY_NON_MOVE(RecvBuffer);
   DcQueueList       Queue_;
   FwdBufferNode*    NodeBack_{nullptr};
   FwdBufferNode*    NodeReserve_{nullptr};
   RecvBufferState   State_{RecvBufferState::NotInUse};

   FwdBufferNode* AllocReserve(size_t expectSize);

public:
   RecvBuffer() {
   }
   ~RecvBuffer() {
      this->Clear();
   }

   static RecvBuffer& StaticCast(DcQueueList& rxbuf) {
      return ContainerOf(rxbuf, &RecvBuffer::Queue_);
   }

   /// 當接收失敗 or 斷線 or 連線成功, 則透過這裡清除: 已收到但未處理的資料.
   void Clear();

   /// 一次取得數個(最多2個)可收資料的空白資料區塊, 全部可用容量必定 >= expectSize.
   /// 透過 fon9_PutIoVectorElement(T* piov, void* dat, size_t datsz); 設定資料區塊位置&大小.
   /// \return 傳回取出的區塊數量.
   template <typename T>
   size_t GetRecvBlockVector(T (&vect)[2], size_t expectSize) {
      assert(this->State_ == RecvBufferState::NotInUse);
      this->State_ = RecvBufferState::Receiving;
      T* piov = vect;
      if ((this->NodeBack_ = FwdBufferNode::CastFrom(this->Queue_.GetBackForExpand())) != nullptr) {
         if (size_t sz = this->NodeBack_->GetRemainSize()) {
            fon9_PutIoVectorElement(piov++, this->NodeBack_->GetDataEnd(), sz);
            if (expectSize <= sz)
               expectSize = 0; // NodeBack_ 剩餘空間足夠 => 不用額外分配 NodeReserve_.
            //else 為了減少重複分配的次數, 所以這裡只用 0 or expectSize 兩種大小: 不考慮還要多少才能達到期望值.
            //   expectSize -= sz;
         }
         else
            this->NodeBack_ = nullptr;
      }
      if (FwdBufferNode* node = this->AllocReserve(expectSize))
         fon9_PutIoVectorElement(piov++, node->GetDataEnd(), node->GetRemainSize());
      return static_cast<size_t>(piov - vect);
   }

   /// 重複使用接收緩衝區: 通常在已不需要「收到資料事件」時使用。
   template <typename T>
   size_t ReuseRecvBlockVector(T (&vect)[2]) {
      assert(this->RecvSt_ == RecvBufferState::Receiving);
      T* piov = vect;
      if (this->NodeBack_)
         fon9_PutIoVectorElement(piov++, this->NodeBack_->GetDataEnd(), this->NodeBack_->GetRemainSize());
      if (this->NodeReserve_)
         fon9_PutIoVectorElement(piov++, this->NodeReserve_->GetDataEnd(), this->NodeReserve_->GetRemainSize());
      return static_cast<size_t>(piov - vect);
   }

   /// 當資料接收完成, 透過這裡設定接收到的資料量, 然後進入 RecvBufferState::InvokingEvent 狀態.
   /// \return 存放接收資料的 DcQueueList.
   DcQueueList& SetDataReceived(size_t rxsz);

   /// 僅能在 OnDevice_Recv() 事件之後呼叫一次.
   /// 傳回之前的狀態.
   RecvBufferState SetContinueRecv() {
      assert(this->IsInvokingEvent());
      RecvBufferState bfst = this->State_;
      this->State_ = RecvBufferState::NotInUse;
      return bfst;
   }

   void SetWaitingEventInvoke() {
      assert(this->State_ == RecvBufferState::InvokingEvent);
      this->State_ = RecvBufferState::WaitingEventInvoke;
   }

   bool IsInvokingEvent() const {
      return(this->State_ >= RecvBufferState::InvokingEvent);
   }
   bool IsWaitingEventInvoke() const {
      return(this->State_ >= RecvBufferState::WaitingEventInvoke);
   }
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_io_RecvBuffer_hpp__
