// \file fon9/buffer/MemBlock_UT.cpp
// \author fonwinz@gmail.com
#include "fon9/buffer/MemBlockImpl.hpp"
#include "fon9/MessageQueue.hpp"
#include "fon9/TestTools.hpp"

//--------------------------------------------------------------------------//

// 使用 malloc()/free() 模擬 fon9::MemBlock.
struct MemAuto {
   fon9_NON_COPY_NON_MOVE(MemAuto);
   void* Mem_{};
public:
   MemAuto() = default;
   explicit MemAuto(size_t sz) : Mem_{malloc(sz)} {
   }
   ~MemAuto() {
      this->Free();
   }

   MemAuto(MemAuto&& r) : Mem_{r.Mem_} {
      r.Mem_ = nullptr;
   }
   MemAuto& operator=(MemAuto&& r) {
      this->Free();
      this->Mem_ = r.Mem_;
      r.Mem_ = nullptr;
      return *this;
   }

   void Alloc(size_t sz) {
      this->Free();
      this->Mem_ = malloc(sz);
   }
   void Free() {
      if (!this->Mem_)
         return;
      free(this->Mem_);
      this->Mem_ = nullptr;
   }
};

//--------------------------------------------------------------------------//

template <class Mem, fon9::MemBlockSize sz>
void TestMemAlloc(const unsigned kTimes, const char* msg) {
   static const unsigned kBlkCount = 1;
   Mem               memvct[kBlkCount];
   fon9::StopWatch   stopWatch;
   stopWatch.ResetTimer();
   for (unsigned L = 0; L < kTimes; ++L) {
      for (unsigned i = 0; i < kBlkCount; ++i)
         memvct[i].Alloc(sz);
      for (unsigned i = 0; i < kBlkCount; ++i)
         memvct[i].Free();
   }
   stopWatch.PrintResult(msg, kTimes);
}

template <class Mem, fon9::MemBlockSize sz>
void TestMemBasket(const unsigned kTimes, const char* msg) {
   std::vector<Mem>  memvec(kTimes);
   fon9::StopWatch   stopWatch;
   stopWatch.ResetTimer();
   for (unsigned L = 0; L < kTimes; ++L)
      memvec[L].Alloc(sz);
   memvec.clear();
   stopWatch.PrintResult(msg, kTimes);
}

fon9_WARN_DISABLE_PADDING;
template <class Mem>
void TestMemThread(const unsigned kTimes, const char* msg) {
   struct MemConsumer {
      using MessageType = Mem;
      using MessageContainer = std::vector<MessageType>;
      MessageContainer ConsumingMessage_;
      using QueueServiceBase = fon9::MessageQueueService<MemConsumer, MessageType, MessageContainer>;
      struct QueueService : public QueueServiceBase {
         fon9_NON_COPY_NON_MOVE(QueueService);
         QueueService() = default;
         std::atomic<size_t>  ConsumedCount_{0};
      };
      QueueService*  QueueService_;
      MemConsumer(QueueServiceBase& qu) : QueueService_{static_cast<QueueService*>(&qu)} {
      }

      //一次處理一筆訊息.
      //void OnMessage(MessageType& mem) {
      //   mem.Free();
      //   ++this->QueueService_->ConsumedCount_;
      //}

      //一次處理一批訊息.
      //void OnMessage(MessageContainer& qu) {
      //   this->QueueService_->ConsumedCount_ += qu.size();
      //   qu.clear();
      //}

      //一次處理一組訊息, 但 MessageContainer 使用 swap() 方式, 可減少 MessageContainer 分配記憶體的負擔.
      void OnMessage(typename QueueService::Locker& queue) {
         this->ConsumingMessage_.swap(*queue);
         queue.unlock();
         this->QueueService_->ConsumedCount_ += this->ConsumingMessage_.size();
         this->ConsumingMessage_.clear();
      }
      void OnThreadEnd(const std::string& thrName) {
         (void)thrName;
      }
   };
   typename MemConsumer::QueueService quService;
   quService.StartThread(1, "MemBlockTester");

   fon9::StopWatch   stopWatch;
   stopWatch.ResetTimer();
   for (size_t L = 0; L < kTimes;) {
      quService.EmplaceMessage(256);
      ++L;
      while (L - quService.ConsumedCount_ > 1000)
         std::this_thread::yield();
   }
   quService.WaitForEndAfterWorkDone();
   stopWatch.PrintResult(msg, kTimes);
}
fon9_WARN_POP;

//--------------------------------------------------------------------------//

int main() {
#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
   fon9::AutoPrintTestInfo utinfo{"Buffer"};
   std::cout << "MemBlock 測試說明:\n"
                "1.MemBlock不是用來取代malloc(), 所以這裡速度的比較意義不大.\n"
                "  但是如果 MemBlock 比 malloc() 慢很多,則要思考 MemBlock 是否有設計上的瑕疵.\n"
                "2.MemBlock對於瞬間大量請求(且沒有Free時), 會有效率上限. 例:\n"
                "  Level=0: size=128B, cache=1024, center=1024*6\n"
                "  如果瞬間 128B 要求超過 1024*7 次, 則會變回使用 malloc()\n"
                "3.MemBlock主要用途是提供 async io 的緩衝區基底.\n"
                "4.此測試的主要目的是驗證正確性\n";

   unsigned idx = 0xff;
   for (unsigned L = 0; L < 0xfffff; ++L) {
      unsigned n = fon9::MemBlockSizeToIndex(L);
      bool isOK = (n >= fon9::MemBlockLevelSize_.size() || L <= fon9::MemBlockLevelSize_[n]);
      if (n != idx || !isOK) {
         if (n != idx) {
            idx = n;
            if (0 < n && n < fon9::kMemBlockLevelCount && isOK && (fon9::MemBlockLevelSize_[n - 1] != (L - 1)))
               isOK = false;
         }
         std::cout << "size=" << std::setw(7) << L << "|index=" << idx;
         if (idx >= fon9::MemBlockLevelSize_.size())
            std::cout << "|use malloc()" << std::endl;
         else {
            std::cout << "|use MemPool.Size=" << std::setw(7) << fon9::MemBlockLevelSize_[idx];
            if (isOK)
               std::cout << "|OK!" << std::endl;
            else {
               std::cout << "|ERROR: incorrect MemPool.Size" << std::endl;
               abort();
            }
         }
      }
   }

   utinfo.PrintSplitter();
   static const unsigned   kTimes = 1000 * 1000;

   // LevelSize=256B: 預留 kListCount 個串列, 每個串列 kTimes / kListCount 個節點.
   static const unsigned   kListCount = 100;
   std::cout << "Prealloc size=" << 256 * kTimes << std::endl;
   fon9::impl::MemBlockInit(256, kListCount, kTimes / kListCount);

   std::cout << "--- (alloc + free)*times ---\n";
   TestMemAlloc<fon9::MemBlock, 256> (kTimes, "MemBlock(256)   ");
   TestMemAlloc<MemAuto, 256>        (kTimes, "malloc          ");
   std::this_thread::sleep_for(std::chrono::seconds(1));

   std::cout << "--- (alloc*times + free*times) ---\n";
   TestMemBasket<fon9::MemBlock, 256>(kTimes, "MemBlock(256)   ");
   TestMemBasket<MemAuto, 256>       (kTimes, "malloc          ");

   std::cout << "--- ThrA:Alloc => ThrB:Free ---\n";
   TestMemThread<fon9::MemBlock>(kTimes, "MemBlock.Thread:");
   TestMemThread<MemAuto>       (kTimes, "malloc.Thread:  ");
}
