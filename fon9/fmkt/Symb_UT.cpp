// \file fon9/fmkt/Symb_UT.cpp
//
// 底下是簡單的測試: Windows + VC:
// 上市+上櫃+期貨+選擇權: 共 24,051 筆資料.
// 可以看出 fon9::Trie 的 find() 所需時間, 約是 std::unordered_map 的 1.8 倍,
// 因此 SymbMap 使用 std::unordered_map 應是較好的選擇.
//
// >Symb_UT 100,6,T30V.TSE.txt 100,6,T30V.OTC.txt 160,10,P08.10 160,10,P08.20
// #####################################################
// fon9 [Symb] test
// =====================================================
// Symb file: T30V.TSE.txt
//     count: 9,140
// Symb file: T30V.OTC.txt
//     count: 2,770
// Symb file: P08.10
//     count: 11,078
// Symb file: P08.20
//     count: 1,063
// ===== fon9::Trie =====
// emplace:    : 0.050116000 secs / 24,051 times =2083.738722    ns|MemUsed(KB)=54,084
// find:       : 0.003751800 secs / 24,051 times = 155.993513783 ns|found=24,051
// ===== std::map =====
// emplace:    : 0.010186600 secs / 24,051 times = 423.541640680 ns|MemUsed(KB)=1,844
// find:       : 0.007187200 secs / 24,051 times = 298.831649412 ns|found=24,051
// ===== std::unordered_map =====
// emplace:    : 0.005640400 secs / 24,051 times = 234.518315247 ns|MemUsed(KB)=2,520
// find:       : 0.002047200 secs / 24,051 times =  85.119121866 ns|found=24,051
// =====================================================
// fon9 [Symb] test # END #
// #####################################################
//
// \author fonwinz@gmail.com
#include "fon9/fmkt/Symb.hpp"
#include "fon9/TestTools.hpp"
#include "fon9/TestTools_MemUsed.hpp"
#include "fon9/File.hpp"

#include "fon9/Trie.hpp"
#include "fon9/SortedVector.hpp"
#include "fon9/DummyMutex.hpp"
#include <map>
#include <mutex>

//--------------------------------------------------------------------------//

// 我們要測試的是 container 的效率, 所以把所有的 Symb 都先載入.
using SymbList = std::vector<fon9::fmkt::SymbSP>;

template <class MapT, class MutexT>
static void Benchmark(const char* benchFor, const SymbList& symbs) {
   //using key_type = typename MapT::key_type;
   using iterator = typename MapT::iterator;
   using Locker = std::unique_lock<MutexT>;

   std::cout << "===== " << benchFor << " =====\n";
   fon9::StopWatch stopWatch;
   MutexT          mx;
   MapT            map;
   uint64_t        memused = GetMemUsed();
   stopWatch.ResetTimer();
   for (const auto& symb : symbs) {
      Locker lk{mx};
      map.emplace(fon9::ToStrView(symb->SymbId_), symb);
   }
   stopWatch.PrintResultNoEOL("emplace:    ", symbs.size()) << "|MemUsed(KB)=" << (GetMemUsed() - memused) << std::endl;

   iterator iend = map.end();
   size_t   found = 0;
   stopWatch.ResetTimer();
   for (const auto& symb : symbs) {
      Locker lk{mx};
      if (map.find(fon9::ToStrView(symb->SymbId_)) != iend)
         ++found;
   }
   stopWatch.PrintResultNoEOL("find:       ", symbs.size()) << "|found=" << found << std::endl;
}
template <class MapT>
static void Benchmark(const char* benchFor, const SymbList& symbs, const char* mx) {
   if (mx && strcmp(mx, "mutex") == 0)
      return Benchmark<MapT, std::mutex>((std::string(benchFor) + "/mutex").c_str(), symbs);
   Benchmark<MapT, fon9::DummyMutex>(benchFor, symbs);
}

//--------------------------------------------------------------------------//

int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

   fon9::AutoPrintTestInfo utinfo{"Symb"};
   const char* iname = nullptr;
   const char* mx = nullptr;
   SymbList    symbs;

   using SymbTrieMap = fon9::fmkt::SymbTrieMap;
   using SymbHashMap = fon9::fmkt::SymbHashMap;
   using SymbSvectMap = fon9::SortedVector<fon9::StrView, fon9::fmkt::SymbSP>;
   using SymbStdMap = std::map<fon9::StrView, fon9::fmkt::SymbSP>;
   for (int n = 1; n < argc; ++n) {
      const char* arg = argv[n];
      if (fon9::isdigit(arg[0])) {
         // RecSize,SymbIdSize,FileName
         size_t   recsz = fon9::StrTo(fon9::StrView_cstr(arg), 0u, &arg);
         if (*arg == 0 || recsz <= 0)
            goto __USAGE;
         size_t   idsz = fon9::StrTo(fon9::StrView_cstr(arg+1), 0u, &arg);
         if (*arg == 0 || idsz <= 0 || idsz > recsz)
            goto __USAGE;
         std::cout << "Symb file: " << ++arg << std::endl;
         fon9::File fd;
         auto       res = fd.Open(arg, fon9::FileMode::Read);
         if (!res) {
            std::cout << "Open error: " << res.GetError().message() << std::endl;
            return 3;
         }
         std::string buf;
         buf.resize(recsz);
         fon9::File::PosType  pos = 0;
         size_t               bfsz = symbs.size();
         for (;;) {
            res = fd.Read(pos, &*buf.begin(), recsz);
            if (!res) {
               std::cout << "Read error: " << res.GetError().message() << std::endl;
               return 3;
            }
            if (res.GetResult() != recsz) {
               if (res.GetResult() == 0) {
                  std::cout << "    count: " << (symbs.size() - bfsz) << std::endl;
                  break;
               }
               std::cout << "Read error:ReqSize=" << recsz << "|ReadSize=" << res.GetResult() << std::endl;
               return 3;
            }
            fon9::StrView symbid{buf.c_str(), idsz};
            symbs.emplace_back(new fon9::fmkt::Symb{fon9::StrTrim(&symbid)});
            pos += recsz;
         }
      }
      else if (strcmp(arg, "mutex") == 0)
         mx = arg;
      else {
         iname = arg;
         if (strcmp(iname, "trie") == 0)
            Benchmark<SymbTrieMap>("fon9::Trie", symbs, mx);
         else if (strcmp(iname, "map") == 0)
            Benchmark<SymbStdMap>("std::map", symbs, mx);
         else if (strcmp(iname, "hash") == 0)
            Benchmark<SymbHashMap>("std::unordered_map", symbs, mx);
         else if (strcmp(iname, "svect") == 0)
            Benchmark<SymbSvectMap>("fon9::SortedVector", symbs, mx);
         else
            goto __USAGE;
      }
   }

   if (iname == nullptr) {
      Benchmark<SymbTrieMap>("fon9::Trie", symbs, mx);
      Benchmark<SymbStdMap>("std::map", symbs, mx);
      Benchmark<SymbHashMap>("std::unordered_map", symbs, mx);
      Benchmark<SymbSvectMap>("fon9::SortedVector", symbs, mx);
   }
   return 0;

__USAGE:
   std::cout << "Usage: RecSize,SymbIdSize,SymbFileName [trie] [map] [hash] [svect]\n";
   return 3;
}
