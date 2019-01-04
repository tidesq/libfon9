/// \file fon9/buffer/BufferNode.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_buffer_BufferNode_hpp__
#define __fon9_buffer_BufferNode_hpp__
#include "fon9/buffer/MemBlock.hpp"
#include "fon9/SinglyLinkedList.hpp"
#include "fon9/Exception.hpp"
#include "fon9/ErrC.hpp"

namespace fon9 {

using BufferNodeSize = MemBlockSize;
class fon9_API BufferNode;
fon9_API void FreeNode(BufferNode* node);

enum class BufferNodeType : BufferNodeSize {
   Data,
   Virtual,
};

/// \ingroup Buffer
/// 一個緩衝區節點.
/// 禁止直接建構&解構.
/// - 透過 FwdBufferNode::Alloc(), RevBufferNode::Alloc() 建立.
/// - 透過 FreeNode() 歸還一個節點.
class fon9_API BufferNode : public SinglyLinkedListNode<BufferNode> {
   fon9_NON_COPY_NON_MOVE(BufferNode);
protected:
   BufferNode() = delete;
   BufferNode(BufferNodeSize blockSize, BufferNodeSize dataOffset)
      : BlockSize_{blockSize}
      , DataBeginOffset_(dataOffset)
      , DataEndOffset_(dataOffset) {
      assert(GetMemBeginOffset() <= dataOffset);
      assert(dataOffset <= blockSize);
   }
   BufferNode(BufferNodeSize blockSize, BufferNodeType nodeType)
      : BlockSize_{blockSize}
      , DataBeginOffset_(static_cast<BufferNodeSize>(nodeType))
      , DataEndOffset_(static_cast<BufferNodeSize>(nodeType)) {
      assert(nodeType != BufferNodeType::Data);
      assert(static_cast<BufferNodeSize>(nodeType) < GetMemBeginOffset());
   }
   ~BufferNode() {
   }

   const BufferNodeSize BlockSize_;
   /// offset from reinterpret_cast<byte*>(this);
   BufferNodeSize       DataBeginOffset_;
   BufferNodeSize       DataEndOffset_;
   /// 實際資料從這裡開始(or 結束:RevBufferNode)存放, 這裡的 [4] 只是為了避免 MSVC 的警告.
   byte                 MemBegin_[4];

   /// 分配一個建議大小的緩衝區區塊.
   /// NodeT 必須提供建構: `NodeT::NodeT(BufferNodeSize blockSize);`
   /// \param  extSize 除了 sizeof(NodeT) 之外, 額外需要的空間: (實際分配的大小 >= extSize + sizeof(NodeT))
   /// \return 必須透過 FreeNode(retval) 歸還.
   template <class NodeT, class... ArgsT>
   static NodeT* Alloc(size_t extSize, ArgsT&&... args) {
      extSize += sizeof(NodeT);
      const MemBlockSize requiredSize = static_cast<MemBlockSize>(extSize);
      if (fon9_UNLIKELY(requiredSize != extSize)) {
         // BufferNode::BufferNodeSize 容納不下 extSize + sizeof(NodeT) !
         Raise<std::length_error>("BufferNode.Alloc");
      }
      MemBlock mb{requiredSize};
      if (void* ptr = mb.begin()) {
         #ifdef new//取消 debug new.
         #undef new
         #endif
            // 為了不輕易讓使用者建立 NodeT, 所以僅允許在 BufferNode::Alloc() 建立, 連使用 InplaceNew<>() 都不允許.
            // InplaceNew<NodeT>(ptr, mb.size());
            NodeT* node = ::new (ptr) NodeT(mb.size(), std::forward<ArgsT>(args)...);
         #ifdef DBG_NEW
         #define new DBG_NEW
         #endif
         if (fon9_UNLIKELY(node->BlockSize_ != mb.size())) {
            // NodeT 的建構, 沒有填入正確的 BlockSize_ ?!
            Raise<std::runtime_error>("BufferNode.Alloc");
         }
         mb.Release();
         return node;
      }
      Raise<std::bad_alloc>();
   }
   static constexpr BufferNodeSize GetMemBeginOffset(const BufferNode* const x = nullptr) {
      return static_cast<BufferNodeSize>(x->MemBegin_ - reinterpret_cast<const byte*>(x));
   }
   const byte* GetMemBegin() const {
      return this->MemBegin_;
   }
   const byte* GetMemEnd() const {
      return reinterpret_cast<const byte*>(this) + this->BlockSize_;
   }
public:
   /// 歸還單一 BufferNode 節點, 不會歸還 node->Next_;
   fon9_API friend void FreeNode(BufferNode* node);

   /// 取得此節點「有效資料」的開始位置.
   byte* GetDataBegin() {
      return reinterpret_cast<byte*>(this) + this->DataBeginOffset_;
   }
   const byte* GetDataBegin() const {
      return reinterpret_cast<const byte*>(this) + this->DataBeginOffset_;
   }
   /// 取得此節點「有效資料」的結束位置.
   byte* GetDataEnd() {
      return reinterpret_cast<byte*>(this) + this->DataEndOffset_;
   }
   const byte* GetDataEnd() const {
      return reinterpret_cast<const byte*>(this) + this->DataEndOffset_;
   }
   /// 取得此節點「有效資料」的資料量(bytes).
   BufferNodeSize GetDataSize() const {
      return this->DataEndOffset_ - this->DataBeginOffset_;
   }
   BufferNodeType GetNodeType() const {
      if (this->DataBeginOffset_ >= GetMemBeginOffset())
         return BufferNodeType::Data;
      return static_cast<BufferNodeType>(this->DataBeginOffset_);
   }

   /// 把資料集中移動到緩衝區尾端.
   /// 這樣可以避免在尾端再加入資料.
   void MoveDataToEnd() {
      if (const BufferNodeSize datsz = this->GetDataSize()) {
         const std::ptrdiff_t  distend = this->GetMemEnd() - this->GetDataEnd();
         if (0 < distend) {
            byte* const pbeg = this->GetDataBegin();
            memmove(pbeg + distend, pbeg, datsz);
         }
      }
   }
   /// 直接設定資料開始位置(偏移量).
   /// - 必須是 data node.
   /// - offset = 移動多少bytes
   void MoveDataBeginOffset(BufferNodeSize offset) {
      assert(this->GetNodeType() == BufferNodeType::Data);
      this->DataBeginOffset_ += offset;
      assert(this->DataBeginOffset_ <= this->DataEndOffset_);
   }
};

/// \ingroup Buffer
/// 計算從 front 開始的資料量(包含front).
inline size_t CalcDataSize(const BufferNode* front) {
   size_t sz = 0;
   while (front) {
      sz += front->GetDataSize();
      front = front->GetNext();
   }
   return sz;
}

fon9_WARN_DISABLE_PADDING;
/// \ingroup Buffer
/// 在 Buffer 消費完此節點時, 會呼叫 OnBufferConsumed(); 然後才會移除&釋放此節點.
/// - 可用於自訂「資料消費之後」的行為.
/// - 用法範例可參考: BufferNodeWaiter, FileAppender::NodeReopen, FileAppender::NodeCheckRotateTime;
/// - 如果是從 io::Device 呼叫 OnBufferConsumed():
///   因此時正鎖定 SendBuffer & OpQueue,
///   所以在這裡呼叫 SendBuffered() 或 OpQueue 會死結!
///   因此若在 OnBufferConsumed() 有 Send 或 OpQueue操作,
///   應移到其他 thread(e.g. GetDefaultThreadPool()) 處理.
class fon9_API BufferNodeVirtual : public BufferNode {
   fon9_NON_COPY_NON_MOVE(BufferNodeVirtual);
   using base = BufferNode;
public:
   enum class StyleFlag {
      /// 允許跨越此節點, 若無此旗標, 則必須在前面的節點都用完後才能繼續。
      AllowCrossing = 0x01,
   };
   const StyleFlag StyleFlags_;

   virtual void OnBufferConsumed() = 0;
   virtual void OnBufferConsumedErr(const ErrC& errc) = 0;

   /// node 必須是 BufferNodeVirtual 才會轉型成功.
   static BufferNodeVirtual* CastFrom(BufferNode* node) {
      return node->GetNodeType() == BufferNodeType::Virtual ? static_cast<BufferNodeVirtual*>(node) : nullptr;
   }

protected:
   BufferNodeVirtual(BufferNodeSize blockSize, StyleFlag style)
      : base(blockSize, BufferNodeType::Virtual)
      , StyleFlags_{style} {
   }
   virtual ~BufferNodeVirtual();

   fon9_API friend void FreeNode(BufferNode* node);
};
fon9_ENABLE_ENUM_BITWISE_OP(BufferNodeVirtual::StyleFlag);
fon9_WARN_POP;

} // namespaces
#endif//__fon9_buffer_BufferNode_hpp__
