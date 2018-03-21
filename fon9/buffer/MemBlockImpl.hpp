/// \file fon9/buffer/MemBlockImpl.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_buffer_MemBlockImpl_hpp__
#define __fon9_buffer_MemBlockImpl_hpp__
#include "fon9/buffer/MemBlock.hpp"
#include "fon9/SinglyLinkedList.hpp"
#include "fon9/SpinMutex.hpp"
#include "fon9/Timer.hpp"
#include <array>

namespace fon9 {

#ifdef _MSC_VER
#pragma intrinsic(_BitScanReverse) 
#endif

inline unsigned MemBlockSizeToIndex(MemBlockSize sz) {
#ifdef _MSC_VER
   unsigned long res;
   if (!_BitScanReverse(&res, (sz - 1) >> 6))
      return 0;
#else
   if ((sz = (sz - 1) >> 6) == 0)
      return 0;
   unsigned res = 31 - __builtin_clz(sz);
#endif
   if (res <= 3) // size <= 1024
      return res;
   return (res >> 1) + 2;
}

enum : unsigned {
   kMemBlockLevelCount = 7
};

constexpr std::array<MemBlockSize, kMemBlockLevelCount> MemBlockLevelSize_{
   128,
   256,
   512,
   1024,
   1024 * 4,
   1024 * 16,
   1024 * 64,
};

//--------------------------------------------------------------------------//

namespace impl {
struct FreeMemNode : public SinglyLinkedListNode<FreeMemNode> {
   fon9_NON_COPY_NON_MOVE(FreeMemNode);
   FreeMemNode() = default;
   inline friend void FreeNode(FreeMemNode* mnode) {
      ::free(mnode);
   }
};
using FreeMemList = SinglyLinkedList<FreeMemNode>;

//--------------------------------------------------------------------------//

enum class TCacheLevelFlag {
   Registered = 0x01,
};
fon9_ENABLE_ENUM_BITWISE_OP(TCacheLevelFlag);

fon9_WARN_DISABLE_PADDING;
struct TCacheLevelPool {
   fon9_NON_COPY_NON_MOVE(TCacheLevelPool);
   TCacheLevelPool() = default;
   FreeMemList       FreeMemCurr_;
   FreeMemList       FreeMemNext_;
   size_t            EmptyCount_{0};
   TCacheLevelFlag   Flags_{};
};
using TCacheLevelPools = std::array<TCacheLevelPool, kMemBlockLevelCount>;

//--------------------------------------------------------------------------//

class MemBlockCenter : public intrusive_ref_counter<MemBlockCenter> {
   fon9_NON_COPY_NON_MOVE(MemBlockCenter);
   class CenterLevelNode : private FreeMemNode {
      fon9_NON_COPY_NON_MOVE(CenterLevelNode);
      using base = SinglyLinkedListNode<CenterLevelNode>;
      CenterLevelNode*  Next_{};
      size_t            FreeMemNodeCount_{};
      friend class SinglyLinkedList<CenterLevelNode>;
      void SetNext(CenterLevelNode* next) {
         this->Next_ = next;
      }
   public:
      CenterLevelNode() = default;
      CenterLevelNode* GetNext() const {
         return this->Next_;
      }

      /// 釋放一個 CenterLevel, 要把 cnode 保留的 FreeMemList 釋放掉.
      inline friend void FreeNode(CenterLevelNode* cnode) {
         FreeMemList autoFree{CenterLevelNode::ToFreeMemList(cnode)};
      }

      static FreeMemList ToFreeMemList(CenterLevelNode* cnode) {
         return !cnode ? FreeMemList{} : FreeMemList{static_cast<FreeMemNode*>(cnode), cnode->FreeMemNodeCount_};
      }

      static CenterLevelNode* FromFreeMemList(FreeMemList&& fmlist) {
         if (size_t count = fmlist.size())
            if (FreeMemNode* fnode = fmlist.ReleaseList()) {
               CenterLevelNode* retval = static_cast<CenterLevelNode*>(fnode);
               retval->SetNext(nullptr);
               retval->FreeMemNodeCount_ = count;
               return retval;
            }
         return nullptr;
      }
   };
   class CenterLevelList : public SinglyLinkedList<CenterLevelNode> {
      fon9_NON_COPYABLE(CenterLevelList);
      using base = SinglyLinkedList<CenterLevelNode>;
   public:
      CenterLevelList() = default;
      CenterLevelList(base&& rhs) : base(std::move(rhs)) {
      }
      CenterLevelList(CenterLevelList&& rhs) = default;
      CenterLevelList& operator=(CenterLevelList&& rhs) = default;
   };
   struct CenterLevelImpl {
      fon9_NON_COPY_NON_MOVE(CenterLevelImpl);
      CenterLevelImpl() = default;
      CenterLevelList   Reserved_;
      CenterLevelList   Recycle_;  // 尚未整理的歸還, 每個 FreeMemList 數量不定.
      size_t            ReservedCount_{3};
      size_t            RequiredCount_{0};
   };
   using CenterLevel = MustLock<CenterLevelImpl, SpinBusy>;

   using CenterLevelArray = std::array<CenterLevel, kMemBlockLevelCount>;
   CenterLevelArray  Levels_;

   static void EmitOnTimer(TimerEntry* timer, TimeStamp now);
   DataMemberEmitOnTimer<&MemBlockCenter::EmitOnTimer> Timer_;
public:
   MemBlockCenter();
   ~MemBlockCenter();

   byte* Alloc(unsigned lvidx, TCacheLevelPool& lv);
   void FreeFull(unsigned lvidx, FreeMemList&& fmlist);
   void Recycle(TCacheLevelPools& levels);
   void InitLevel(unsigned lvidx, const size_t* reserveFreeListCount);
};
fon9_WARN_POP;

fon9_API bool MemBlockInit(MemBlockSize size, size_t reserveFreeListCount, size_t maxNodeCount);
} // namespace impl
} // namespace fon9
#endif//__fon9_buffer_MemBlockImpl_hpp__
