/// \file fon9/buffer/DcQueueList.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_buffer_DcQueueList_hpp__
#define __fon9_buffer_DcQueueList_hpp__
#include "fon9/buffer/DcQueue.hpp"
#include "fon9/buffer/BufferList.hpp"

#ifdef fon9_POSIX
#define fon9_HAVE_iovec
#include <sys/uio.h> // struct iovec
#include <limits.h>  // IOV_MAX
inline void fon9_PutIoVectorElement(struct iovec* piov, void* dat, size_t datsz) {
   piov->iov_base = dat;
   piov->iov_len = datsz;
}
#endif

namespace fon9 {

fon9_MSC_WARN_DISABLE(4251  //C4251: 'BlockList_': class 'SinglyLinkedList2<fon9::BufferNode>' needs to have dll-interface
                      4265);// dtor isnot virtual.
/// \ingroup Buffer
/// 訊息消費端的緩衝區處理: 使用 BufferList.
class fon9_API DcQueueList : public DcQueue {
   fon9_NON_COPYABLE(DcQueueList);
   using base = DcQueue;
protected:
   BufferList  BlockList_;

   void NodeConsumed(BufferNode* node) {
      if (BufferNodeVirtual* vnode = BufferNodeVirtual::CastFrom(node))
         vnode->OnBufferConsumed();
      FreeNode(node);
   }
   void NodeConsumed(BufferNode* node, const ErrC& errc, BufferNodeSize errSize) {
      (void)errSize; // node 使用失敗的資料量.
      if (BufferNodeVirtual* vnode = BufferNodeVirtual::CastFrom(node))
         vnode->OnBufferConsumedErr(errc);
      FreeNode(node);
   }

   void FrontToCurrBlock();

   virtual bool DcQueuePeekMore(byte* tmpbuf, size_t sz) override;
   virtual bool DcQueueHasMore(size_t sz) const override;
   /// 移除已用掉的資料量.
   /// 歸還已用掉的 BufferNode, 如果有控制節點, 則會觸發 OnBufferConsumed() 通知.
   virtual void DcQueueRemoveMore(size_t sz) override;
   virtual size_t DcQueueReadMore(byte* buf, size_t sz) override;
public:
   DcQueueList() = default;
   explicit DcQueueList(BufferList&& buf) : base(), BlockList_{std::move(buf)} {
      this->FrontToCurrBlock();
   }
   DcQueueList(DcQueueList&& rhs) : base(rhs.MemCurrent_,rhs.MemEnd_), BlockList_{std::move(rhs.BlockList_)} {
      rhs.ClearCurrBlock();
   }
   DcQueueList& operator=(DcQueueList&& rhs) = delete;

   /// 剩餘資料使用 this->ConsumeErr(std::errc::operation_canceled); 處理.
   ~DcQueueList() {
      if (!this->BlockList_.empty())
         this->ConsumeErr(std::errc::operation_canceled);
   }

   /// 消費資料時發生錯誤, 清除緩衝區內的全部資料(如果有 BufferNodeCallback, 則會自動觸發通知).
   virtual void ConsumeErr(const ErrC& errc) override;

   /// 將 node 加到尾端, 並交由 this 管理.
   /// node 不可為 nullptr.
   void push_back(BufferNode* node) {
      assert(node != nullptr);
      this->BlockList_.push_back(node);
      this->FrontToCurrBlock();
   }
   void push_back(BufferList&& buf) {
      this->BlockList_.push_back(std::move(buf));
      this->FrontToCurrBlock();
   }
   /// 取得第一個節點.
   const BufferNode* cfront() const {
      return this->BlockList_.front();
   }
   /// 移出 BufferList.
   BufferList MoveOut();

   virtual size_t CalcSize() const override {
      if (const BufferNode* node = this->BlockList_.front())
         return this->GetCurrBlockSize() + CalcDataSize(node->GetNext());
      return 0;
   }
   /// 一次取得數個資料區塊.
   /// 透過 fon9_PutIoVectorElement(T* piov, void* dat, size_t datsz); 設定資料區塊位置&大小.
   /// \return 傳回取出的區塊數量.
   template <typename T, size_t maxCount>
   size_t PeekBlockVector(T (&vect)[maxCount]) {
      if (this->MemCurrent_ == nullptr)
         return 0;
      fon9_PutIoVectorElement(vect, const_cast<byte*>(this->MemCurrent_), this->GetCurrBlockSize());
      size_t      count = 1;
      BufferNode* node = this->BlockList_.front();
      while(count < maxCount) {
         if ((node = node->GetNext()) == nullptr)
            break;
         if (const size_t blksz = node->GetDataSize())
            fon9_PutIoVectorElement(vect + count++, node->GetDataBegin(), blksz);
         else if (BufferNodeVirtual* vnode = BufferNodeVirtual::CastFrom(node)) {
            // 是否允許跨越控制節點?
            if (!IsEnumContains(vnode->StyleFlags_, BufferNodeVirtual::StyleFlag::AllowCrossing))
               break;
         }
      }
      return count;
   }
};
fon9_MSC_WARN_POP;

/// \ingroup Buffer
/// - DcQueueList 必須支援: PeekBlockVector();
/// - 必須額外提供 fon9_PutIoVectorElement(IoVec* iov, void* dat, size_t size); 將 dat,size 填入 iov;
template <class DcQueueList, class IoVec, size_t IoVecMaxN, class FnWriter>
auto DeviceOutputIovec(DcQueueList& buf, IoVec (&iov)[IoVecMaxN], FnWriter fnWriter) -> decltype(fnWriter(nullptr, 0)) {
   using Outcome = decltype(fnWriter(nullptr, 0));
   typename Outcome::ResultType  wsz{0};
   for (;;) {
      size_t iovcnt = buf.PeekBlockVector(iov);
      if (iovcnt <= 0)
         return Outcome{wsz};
      Outcome res = fnWriter(iov, iovcnt);
      if (!res) {
         buf.ConsumeErr(res.GetError());
         return res;
      }
      wsz += res.GetResult();
      buf.PopConsumed(res.GetResult());
   }
}

#ifdef fon9_HAVE_iovec
template <class DcQueueList, class FnWriter>
auto DeviceOutputIovec(DcQueueList& buf, FnWriter fnWriter) -> decltype(fnWriter(nullptr, 0)) {
   struct iovec   iov[IOV_MAX];
   return DeviceOutputIovec(buf, iov, fnWriter);
}
#endif

inline void BufferListConsumeErr(BufferList&& src, const ErrC& errc) {
   DcQueueList{std::move(src)}.ConsumeErr(errc);
}

} // namespaces
#endif//__fon9_buffer_DcQueueList_hpp__
