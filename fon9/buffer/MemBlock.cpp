// \file fon9/buffer/MemBlock.hpp
// \author fonwinz@gmail.com
#include "fon9/buffer/MemBlockImpl.hpp"
#include "fon9/StaticPtr.hpp"

namespace fon9 {

static std::array<size_t, kMemBlockLevelCount> MemBlockLevelMaxNodeCount_{
   128,  // [0]: Size=128, 通常用在 BufferNodeVirtual.
   1024, // [1]: Size=256, 通常用在 Log: RevBufferList rbuf{kLogBlockNodeSize};
   1024, // [2]: Size=512, 通常用在 FIX Message builder.
   32,   // [3]: Size=1024, 通常用在 device input.
   32,   // [4]: Size=4096
   32,   // [5]: Size=16K
   32,   // [6]: Size=64K
};

//--------------------------------------------------------------------------//
namespace impl {
// kMemBlockCenter_CheckInterval: MemBlock 整理間隔。
// 不用太頻繁，因為若瞬間有大用量，則應暫時保留較多的緩衝。若用量不大，也沒必要頻繁的整理。
static const TimeInterval  kMemBlockCenter_CheckInterval{TimeInterval_Millisecond(2000)};

MemBlockCenter::MemBlockCenter() : Timer_{GetDefaultTimerThread()} {
   Timer_.RunAfter(TimeInterval{});
}
MemBlockCenter::~MemBlockCenter() {
   this->Timer_.DisposeAndWait();
}

byte* MemBlockCenter::Alloc(unsigned lvidx, TCacheLevelPool& lv) {
   assert(lv.FreeMemCurr_.empty() && lv.FreeMemNext_.empty());
   CenterLevel::Locker  lvCenter{this->Levels_[lvidx]};
   if (!IsEnumContains(lv.Flags_, TCacheLevelFlag::Registered)) {
      lv.Flags_ |= TCacheLevelFlag::Registered;
      ++lvCenter->RequiredCount_;
   }
   CenterLevelNode* cnode = lvCenter->Reserved_.pop_front();
   if (cnode == nullptr)
      cnode = lvCenter->Recycle_.pop_front();
   lvCenter.unlock();
   if (cnode == nullptr)
      return nullptr;
   /// 由於採用 [定時檢查] 的方式補充 CenterLevelList, 所以這裡不用再觸發任何通知.
   /// 這樣可避免: 大用量(無歸還or在另一thread歸還)時, 此處會不斷的觸發[喚醒檢查this->Levels_], 反而造成效率問題!
   lv.FreeMemCurr_ = CenterLevelNode::ToFreeMemList(cnode);
   return reinterpret_cast<byte*>(lv.FreeMemCurr_.pop_front());
}
void MemBlockCenter::FreeFull(unsigned lvidx, FreeMemList&& fmlist) {
   if (CenterLevelNode* cnode = CenterLevelNode::FromFreeMemList(std::move(fmlist))) {
      CenterLevel::Locker lvCenter{this->Levels_[lvidx]};
      lvCenter->Reserved_.push_front(cnode);
   }
}
void MemBlockCenter::Recycle(TCacheLevelPools& levels) {
   unsigned lvidx = 0;
   for (TCacheLevelPool& lv : levels) {
      if (IsEnumContains(lv.Flags_, TCacheLevelFlag::Registered)) {
         lv.Flags_ -= TCacheLevelFlag::Registered;
         CenterLevelNode*    cnode1 = CenterLevelNode::FromFreeMemList(std::move(lv.FreeMemCurr_));
         CenterLevelNode*    cnode2 = CenterLevelNode::FromFreeMemList(std::move(lv.FreeMemNext_));
         CenterLevel::Locker lvCenter{this->Levels_[lvidx]};
         if (cnode1)
            lvCenter->Recycle_.push_front(cnode1);
         if (cnode2)
            lvCenter->Recycle_.push_front(cnode2);
         --lvCenter->RequiredCount_;
         if (cnode1 || cnode2)
            this->InitLevel(lvidx, lvCenter);
      }
      ++lvidx;
   }
}

static bool FreeMemListMerge(FreeMemList& dst, FreeMemList& src, size_t maxNodeCount) {
   while (dst.size() < maxNodeCount) {
      if (FreeMemNode* mnode = src.pop_front())
         dst.push_front(mnode);
      else
         return false;
   }
   return true;
}
void MemBlockCenter::InitLevel(unsigned lvidx, CenterLevel::Locker& lvCenter) {
   size_t   count = (lvCenter->ReservedCount_ + lvCenter->RequiredCount_);
   size_t   curr = lvCenter->Reserved_.size();
   CenterLevelList recycle = std::move(lvCenter->Recycle_);
   if (curr > count) {
      CenterLevelList r2 = lvCenter->Reserved_.pop_front(curr - count);
      lvCenter.unlock();
      // auto free: r2, recycle;
      return;
   }
   lvCenter.unlock();

   const size_t maxNodeCount = MemBlockLevelMaxNodeCount_[lvidx];
   FreeMemList  fmlist{CenterLevelNode::ToFreeMemList(recycle.pop_front())};
   for (; curr < count; ++curr) {
      FreeMemList fmlist2{CenterLevelNode::ToFreeMemList(recycle.pop_front())};
      if (!FreeMemListMerge(fmlist, fmlist2, maxNodeCount)) {
         do {
            fmlist.push_front(InplaceNew<FreeMemNode>(malloc(MemBlockLevelSize_[lvidx])));
         } while (fmlist.size() < maxNodeCount);
      }
      CenterLevelNode* cnode = CenterLevelNode::FromFreeMemList(std::move(fmlist));
      fmlist = std::move(fmlist2);
      lvCenter.lock();
      lvCenter->Reserved_.push_front(cnode);
      curr = lvCenter->Reserved_.size();
      count = (lvCenter->ReservedCount_ + lvCenter->RequiredCount_);
      if (recycle.empty())
         recycle = std::move(lvCenter->Recycle_);
      lvCenter.unlock();
   }
}
void MemBlockCenter::InitLevel(unsigned lvidx, const size_t* reserveFreeListCount) {
   CenterLevel::Locker lvCenter{this->Levels_[lvidx]};
   if (reserveFreeListCount)
      lvCenter->ReservedCount_ = *reserveFreeListCount;
   this->InitLevel(lvidx, lvCenter);
}

void MemBlockCenter::EmitOnTimer(TimerEntry* timer, TimeStamp) {
   MemBlockCenter& rthis = ContainerOf(*static_cast<decltype(MemBlockCenter::Timer_)*>(timer), &MemBlockCenter::Timer_);
   for (unsigned lvidx = 0; lvidx < kMemBlockLevelCount; ++lvidx)
      rthis.InitLevel(lvidx, nullptr);
   timer->RunAfter(kMemBlockCenter_CheckInterval);
}

using MemBlockCenterSP = intrusive_ptr<MemBlockCenter>;
static MemBlockCenterSP GetMemBlockCenter() {
   static MemBlockCenterSP MemBlockCenter{new impl::MemBlockCenter{}};
   // 如果系統正在結束 MemBlockCenter 已死, 此時應傳回 nullptr, 然後使用 MemBlock::UseMalloc();
   return MemBlockCenter->use_count() ? MemBlockCenter : nullptr;
}

fon9_API bool MemBlockInit(MemBlockSize size, size_t reserveFreeListCount, size_t maxNodeCount) {
   unsigned lvidx = MemBlockSizeToIndex(size);
   if (lvidx >= kMemBlockLevelCount)
      return false;
   if (MemBlockLevelMaxNodeCount_[lvidx] < maxNodeCount)
      MemBlockLevelMaxNodeCount_[lvidx] = maxNodeCount;
   GetMemBlockCenter()->InitLevel(lvidx, &reserveFreeListCount);
   return true;
}
} // namespace impl
using namespace impl;

//--------------------------------------------------------------------------//

class MemBlock::TCache {
   fon9_NON_COPY_NON_MOVE(TCache);
   TCacheLevelPools        Levels_;
public:
   const MemBlockCenterSP  Center_;
   TCache() : Center_{GetMemBlockCenter()} {
   }
   ~TCache() {
      this->Center_->Recycle(this->Levels_);
   }
   static byte* UseMalloc(MemBlock& mblk, MemBlockSize sz) {
      if (fon9_UNLIKELY(mblk.MemPtr_))
         free(mblk.MemPtr_);
      if (fon9_LIKELY((mblk.Size_ = -static_cast<SSizeT>(sz)) <= 0))
         if (fon9_LIKELY((mblk.MemPtr_ = reinterpret_cast<byte*>(malloc(sz))) != nullptr))
            return mblk.MemPtr_;
      mblk.Size_ = 0;
      return mblk.MemPtr_ = nullptr;
   }
   byte* Alloc(MemBlock& mblk, MemBlockSize newsz) {
      const MemBlockSize oldsz = mblk.size();
      if (oldsz >= newsz)
         return mblk.begin();
      const unsigned lvidx = MemBlockSizeToIndex(newsz);
      if (lvidx >= kMemBlockLevelCount)
         return this->UseMalloc(mblk, newsz);

      this->Free(mblk.Release(), oldsz);
      newsz = MemBlockLevelSize_[lvidx];
      TCacheLevelPool& lv = this->Levels_[lvidx];
      if (lv.FreeMemCurr_.empty())
         lv.FreeMemCurr_ = std::move(lv.FreeMemNext_);
      byte* pmem = reinterpret_cast<byte*>(lv.FreeMemCurr_.pop_front());
      if (fon9_UNLIKELY(pmem == nullptr)) {
         if ((pmem = this->Center_->Alloc(lvidx, lv)) == nullptr) {
            ++lv.EmptyCount_;
            if ((pmem = static_cast<byte*>(malloc(newsz))) == nullptr)
               return nullptr;
         }
      }
      mblk.Size_ = static_cast<SSizeT>(newsz);
      return mblk.MemPtr_ = pmem;
   }
   void Free(void* ptr, MemBlockSize sz) {
      if (fon9_UNLIKELY(ptr == nullptr))
         return;
      const unsigned lvidx = MemBlockSizeToIndex(sz);
      if (fon9_UNLIKELY(lvidx >= kMemBlockLevelCount)) {
         ::free(ptr);
         return;
      }
      assert(sz == MemBlockLevelSize_[lvidx]);
      // 重建 node = 初始值. 然後放入 list.
      FreeMemNode*      node = InplaceNew<FreeMemNode>(ptr);
      const size_t      maxNodeCount = MemBlockLevelMaxNodeCount_[lvidx];
      TCacheLevelPool&  lv = this->Levels_[lvidx];
      FreeMemList*      fmlist = (lv.FreeMemCurr_.size() < maxNodeCount ? &lv.FreeMemCurr_
                                  : lv.FreeMemNext_.size() < maxNodeCount ? &lv.FreeMemNext_
                                  : nullptr);
      if (fon9_LIKELY(fmlist)) {
         fmlist->push_front(node);
         return;
      }
      this->Center_->FreeFull(lvidx, std::move(lv.FreeMemCurr_));
      lv.FreeMemCurr_.push_front(node);
   }
};
static thread_local StaticPtr<MemBlock::TCache> TCache_;

//--------------------------------------------------------------------------//

byte* MemBlock::Alloc(MemBlockSize sz) {
   if (fon9_LIKELY(TCache_)) {
__TCACHE_READY:
      return TCache_->Alloc(*this, sz);
   }
   if (!TCache_.IsDisposed()) {
      TCache_.reset(new MemBlock::TCache);
      if (TCache_->Center_)
         goto __TCACHE_READY;
      TCache_.dispose();
   }
   return TCache::UseMalloc(*this, sz);
}
void MemBlock::FreeBlock(void* mem, MemBlockSize sz) {
   if (fon9_LIKELY(mem)) {
      if (fon9_LIKELY(TCache_))
         TCache_->Free(mem, sz);
      else
         free(mem);
   }
}

} // namespace fon9
