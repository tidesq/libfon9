// \file fon9/Trie_UT.cpp
// \author fonwinz@gmail.com
#include "fon9/Trie.hpp"
#include "fon9/CharAry.hpp"
#include "fon9/StrTo.hpp"
#include "fon9/TestTools.hpp"

#include <map>
#include <unordered_map>

#ifdef fon9_WINDOWS
fon9_BEFORE_INCLUDE_STD;
#include <psapi.h> // for memory usage.
fon9_AFTER_INCLUDE_STD;
#endif

//--------------------------------------------------------------------------//

using StrTrie = fon9::Trie<fon9::TrieKeyAlNum, std::string>;

std::string GetKey(const StrTrie::iterator& i) {
   return i->key();
}
std::string& GetValue(const StrTrie::iterator& i) {
   return i->value();
}
bool operator!=(fon9::StrView lhs, const std::string& rhs) {
   return lhs != fon9::ToStrView(rhs);
}

//--------------------------------------------------------------------------//

struct StdKey : public fon9::CharAry<5> {
   using base = fon9::CharAry<5>;
   template <size_t arysz>
   StdKey(const char(&ary)[arysz]) : base{ary} {}
   StdKey(const char* ibeg, size_t sz) : base{fon9::StrView{ibeg, sz}} {}
   StdKey() : base{""} {}

   bool operator!=(const char* rhs) const {
      return ToStrView(*this) != fon9::StrView_cstr(rhs);
   }
   bool operator!=(const std::string& rhs) const {
      return ToStrView(*this) != fon9::ToStrView(rhs);
   }
};

using StdMap = std::map<StdKey, std::string>;

std::string GetKey(const StdMap::iterator& i) {
   return ToStrView(i->first).ToString();
}
std::string& GetValue(const StdMap::iterator& i) {
   return i->second;
}

//--------------------------------------------------------------------------//

namespace std {
template<> struct hash<StdKey> {
   size_t operator()(const StdKey& val) const {
      size_t h = 0;
      for (char k : val)
         h = h * fon9::kSeq2AlphaSize + fon9::Alpha2Seq(k);
      return h;
   }
};
} // namespace std

template <class MapT>
void GetLowerBoundType(...);

template <class MapT>
auto GetLowerBoundType(int) -> decltype(static_cast<MapT*>(nullptr)->lower_bound(typename MapT::key_type{}));

template <class MapT>
struct TestHasLowerBound {
   enum { HasLowerBound = !std::is_same<decltype(GetLowerBoundType<MapT>(1)), void>::value };
};

std::ostream& operator<<(std::ostream& os, fon9::StrView strv) {
   return os << strv.ToString();
}
std::ostream& operator<<(std::ostream& os, const StdKey& key) {
   return os << fon9::ToStrView(key).ToString();
}

//--------------------------------------------------------------------------//

template <class MapT>
struct AuxTest {
   using key_type = typename MapT::key_type;
   using iterator = typename MapT::iterator;

   template <class ExpectKeyT>
   static void CheckResult(const char* strFuncName, const key_type& key, iterator ifind, bool isFound, const ExpectKeyT* expectKey) {
      std::cout << "[TEST ] " << strFuncName << "()|key=" << key;
      if (isFound)
         std::cout << "|ikey=" << GetKey(ifind) << "|value=" << GetValue(ifind);
      else
         std::cout << "|Not found.";

      if (isFound != (expectKey != nullptr))
         std::cout << "|find result not expect!";
      else if (isFound && *expectKey != GetKey(ifind))
         std::cout << "|key not match!";
      else {
         std::cout << "\r[OK   ]" << std::endl;
         return;
      }
      std::cout << "\r[ERROR]" << std::endl;
      abort();
   }

   static void TestFind(MapT& map, const key_type& key, bool isFindExpect) {
      iterator ifind = map.find(key);
      bool     isFound = (ifind != map.end());
      CheckResult("find", key, ifind, isFound, isFindExpect ? &key : nullptr);
   }
   static void CheckBoundResult(const char* strFuncName, MapT& map, iterator ifind, const key_type& key, const char* cstrExpectKey) {
      std::string expectKey{cstrExpectKey ? cstrExpectKey : ""};
      CheckResult(strFuncName, key, ifind, (ifind != map.end()), cstrExpectKey ? &expectKey : nullptr);
   }
   static void TestLowerBound(MapT& map, const key_type& key, const char* findVal) {
      CheckBoundResult("lower_bound", map, map.lower_bound(key), key, findVal);
   }
   static void TestUpperBound(MapT& map, const key_type& key, const char* findVal) {
      CheckBoundResult("upper_bound", map, map.upper_bound(key), key, findVal);
   }

   static void InitTest(MapT& map) {
      map.emplace("00x1", "value1");
      map.emplace("01x2", "value2");
      map.emplace("xAxA", "value3");
      map.emplace("xaxA", "value4");
      if (map.size() != 4 || map.empty()) {
         std::cout << "|size(),empty() Not match!" << std::endl;
         abort();
      }
   }
   static void TestFind() {
      MapT map;
      InitTest(map);
      TestFind(map, "00", false);
      TestFind(map, "00x0", false);
      TestFind(map, "00x1", true);
      TestFind(map, "00x2", false);

      TestFind(map, "01x1", false);
      TestFind(map, "01x2", true);
      TestFind(map, "01x3", false);

      TestFind(map, "xax9", false);
      TestFind(map, "xaxA", true);
      TestFind(map, "xaxB", false);
   }
   static void TestLowerBound() {
      MapT map;
      InitTest(map);
      TestLowerBound(map, "00", "00x1");
      TestLowerBound(map, "00x0", "00x1");
      TestLowerBound(map, "00x1", "00x1");

      TestLowerBound(map, "00x2", "01x2");
      TestLowerBound(map, "01x1", "01x2");
      TestLowerBound(map, "01x2", "01x2");

      TestLowerBound(map, "01x3", "xAxA");
      TestLowerBound(map, "xax9", "xaxA");
      TestLowerBound(map, "xaxA", "xaxA");
      TestLowerBound(map, "xaxB", nullptr);
   }
   static void TestUpperBound() {
      MapT map;
      InitTest(map);
      TestUpperBound(map, "00", "00x1");
      TestUpperBound(map, "00x0", "00x1");

      TestUpperBound(map, "00x1", "01x2");
      TestUpperBound(map, "00x2", "01x2");
      TestUpperBound(map, "01x1", "01x2");

      TestUpperBound(map, "01x2", "xAxA");
      TestUpperBound(map, "01x3", "xAxA");
      TestUpperBound(map, "xax9", "xaxA");
      TestUpperBound(map, "xaxA", nullptr);
      TestUpperBound(map, "xaxB", nullptr);
   }

   static void CheckEmpty(MapT& map) {
      if (map.size() != 0 || !map.empty()) {
         std::cout << "|Not empty!" << "\r[ERROR]" << std::endl;
         abort();
      }
      std::cout << "\r[OK   ]" << std::endl;
   }

   static uint64_t GetMemUsed() {
   #ifdef fon9_WINDOWS
      PROCESS_MEMORY_COUNTERS_EX pmc;
      GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc));
      return pmc.PrivateUsage / 1024;
   #else
      struct Aux {
         static uint64_t parseLine(char* line) {
            // This assumes that a digit will be found and the line ends in " Kb".
            const char* p = line;
            while (*p <'0' || *p > '9')
               ++p;
            return fon9::NaiveStrToUInt(fon9::StrView{p, line + strlen(line)});
         }
         static uint64_t getValue() { //Note: this value is in KB!
            FILE* file = fopen("/proc/self/status", "r");
            char line[128];
            while (fgets(line, 128, file) != NULL) {
               if (memcmp(line, "VmSize:", 7) == 0)
                  return parseLine(line);
            }
            fclose(file);
            return 0;
         }
      };
      return Aux::getValue();
   #endif
   }

   static key_type MakeKey(char(&key)[5], uint64_t i) {
      for (int L = sizeof(key); L > 0;) {
         key[--L] = fon9::Seq2Alpha(static_cast<uint8_t>(i % fon9::kSeq2AlphaSize));
         i /= fon9::kSeq2AlphaSize;
      }
      return key_type{key,sizeof(key)};
   }
#ifdef _DEBUG
   static const uint64_t   kTimes = 1000;
#else
   static const uint64_t   kTimes = 1000 * 1000;
#endif
   static void Benchmark(const char* benchFor) {
      std::cout << "===== " << benchFor << " / sequence =====\n";
      MapT  map;
      BenchmarkEmplace(map, 1);
      BenchmarkBound(map, 1);
      BenchmarkClear(map);

      std::cout << "----- random -----\n";
      static const uint64_t rndseed = 7654321;
      BenchmarkEmplace(map, rndseed);
      BenchmarkBound(map, rndseed);
      BenchmarkClear(map);
   }
   static void BenchmarkEmplace(MapT& map, uint64_t rndseed) {
      fon9::StopWatch stopWatch;
      char     key[5];
      uint64_t memused = GetMemUsed();
      stopWatch.ResetTimer();
      for (uint64_t L = 0; L < kTimes; ++L)
         map.emplace(MakeKey(key, L * rndseed), std::string{});
      stopWatch.PrintResultNoEOL("emplace:    ", kTimes) << "|MemUsed(KB)=" << (GetMemUsed() - memused) << std::endl;

      iterator iend = map.end();
      size_t   found = 0;
      stopWatch.ResetTimer();
      for (uint64_t L = 0; L < kTimes; ++L) {
         if (map.find(MakeKey(key, L * rndseed)) != iend)
            ++found;
      }
      stopWatch.PrintResultNoEOL("find:       ", kTimes) << "|found=" << found << std::endl;
   }
   static void BenchmarkClear(MapT& map) {
      fon9::StopWatch stopWatch;
      map.clear();
      stopWatch.PrintResult("clear:      ", kTimes);
   }

   template <class AutoMapT>
   static fon9::enable_if_t<TestHasLowerBound<AutoMapT>::HasLowerBound> BenchmarkBound(AutoMapT& map, uint64_t rndseed) {
      fon9::StopWatch stopWatch;
      char key[5];
      for (uint64_t L = 0; L < kTimes; ++L)
         map.lower_bound(MakeKey(key, L * rndseed));
      stopWatch.PrintResult("lower_bound:", kTimes);

      stopWatch.ResetTimer();
      for (uint64_t L = 0; L < kTimes; ++L)
         map.upper_bound(MakeKey(key, L * rndseed));
      stopWatch.PrintResult("upper_bound:", kTimes);
   }
   template <class AutoMapT>
   static fon9::enable_if_t<!TestHasLowerBound<AutoMapT>::HasLowerBound> BenchmarkBound(AutoMapT&, uint64_t) {
   }
};

//--------------------------------------------------------------------------//

static void TestFindTail(StrTrie& trie, const StrTrie::key_type& keyHead, const StrTrie::keystr_type& exp) {
   using keystr = typename StrTrie::keystr_type;
   keystr out;
   trie.find_tail_keystr(keyHead, out);
   std::cout << "[TEST ] find_tail_keystr()|keyHead=" << keyHead.ToString() << "|out=" << out << "|exp=" << exp;
   if (out != exp) {
      std::cout << "|Not match!" << "\r[ERROR]" << std::endl;
      abort();
   }
   std::cout << "\r[OK   ]" << std::endl;
}

static void TestFindTail() {
   StrTrie trie;
   AuxTest<StrTrie>::InitTest(trie);
   TestFindTail(trie, "A", "");
   TestFindTail(trie, "0", "01x2");
   TestFindTail(trie, "01", "01x2");
   TestFindTail(trie, "02", "0");
   TestFindTail(trie, "x", "xaxA");
}

//--------------------------------------------------------------------------//

int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
   fon9::AutoPrintTestInfo utinfo{"Trie"};

   using AuxTrie = AuxTest<StrTrie>;
   StrTrie trie;
   std::cout << "[TEST ] count=" << trie.size() << "|empty=" << trie.empty();
   AuxTrie::CheckEmpty(trie);

   AuxTrie::InitTest(trie);
   std::cout << "[INIT ] count=" << trie.size() << "|empty=" << trie.empty();
   for (StrTrie::value_type& v : trie)
      std::cout << '|' << v.key() << "=" << v.value();
   std::cout << std::endl;
   
   // 用 std map, 驗證 test case 是否正確.
   using AuxStd = AuxTest<StdMap>;
   AuxStd::TestFind();
   AuxStd::TestLowerBound();
   AuxStd::TestUpperBound();

   AuxTrie::TestFind();
   AuxTrie::TestLowerBound();
   AuxTrie::TestUpperBound();
   TestFindTail();

   std::cout << "Erasing all...\n";
   StrTrie::iterator i = trie.begin();
   while (!trie.empty())
      i = trie.erase(i);
   std::cout << "[TEST ] count=" << trie.size() << "|empty=" << trie.empty();
   AuxTrie::CheckEmpty(trie);

   AuxTrie::InitTest(trie);
   while (!trie.empty()) {
      i = trie.end();
      trie.erase(--i);
   }
   std::cout << "[TEST ] count=" << trie.size() << "|empty=" << trie.empty();
   AuxTrie::CheckEmpty(trie);

   utinfo.PrintSplitter();
   const char* iname = (argc >= 2 ? argv[1] : nullptr);
   if (iname == nullptr || strcmp(iname, "map") == 0)
      AuxStd::Benchmark("std::map");

   if (iname == nullptr || strcmp(iname, "trie") == 0)
      AuxTrie::Benchmark("fon9::Trie");

   using StdUno = std::unordered_map<StdKey, std::string>;
   using AuxUno = AuxTest<StdUno>;
   if (iname == nullptr || strcmp(iname, "hash") == 0)
      AuxUno::Benchmark("std::unordered_map");
}
