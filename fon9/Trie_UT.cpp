// \file fon9/Trie_UT.cpp
// \author fonwinz@gmail.com
#include "fon9/Trie.hpp"
#include "fon9/CharAry.hpp"
#include "fon9/StrTo.hpp"
#include "fon9/TestTools.hpp"
#include "fon9/TestTools_MemUsed.hpp"

#include <map>
#include <unordered_map>
#include "fon9/SortedVector.hpp"

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

struct SortedVector : public fon9::SortedVector<StdKey, std::string> {
   iterator emplace(StdKey key, std::string value) {
      return this->insert(value_type{std::move(key),std::move(value)}).first;
   }
};

std::string GetKey(const SortedVector::iterator& i) {
   return ToStrView(i->first).ToString();
}
std::string& GetValue(const SortedVector::iterator& i) {
   return i->second;
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
   using AuxStd = AuxTest<StdMap>;
   if (argc < 2) {
      StrTrie trie;
      std::cout << "[TEST ] count=" << trie.size() << "|empty=" << trie.empty();
      AuxTrie::CheckEmpty(trie);

      AuxTrie::InitTest(trie);
      std::cout << "[INIT ] count=" << trie.size() << "|empty=" << trie.empty();
      for (StrTrie::value_type& v : trie)
         std::cout << '|' << v.key() << "=" << v.value();
      std::cout << std::endl;

      // 用 std map, 驗證 test case 是否正確.
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
   }
   const char* iname = (argc >= 2 ? argv[1] : nullptr);
   if (iname == nullptr || strcmp(iname, "map") == 0)
      AuxStd::Benchmark("std::map");

   if (iname == nullptr || strcmp(iname, "trie") == 0)
      AuxTrie::Benchmark("fon9::Trie");

   using StdUno = std::unordered_map<StdKey, std::string>;
   using AuxUno = AuxTest<StdUno>;
   if (iname == nullptr || strcmp(iname, "hash") == 0)
      AuxUno::Benchmark("std::unordered_map");

   // 在 Linux(Ubuntu 16.04.3) + gcc(5.4.0 20160609); 
   // ===== std::map / sequence =====
   // emplace:    : 0.867709394 secs / 1,000,000 times = 867.709394000 ns
   // find:       : 0.567231606 secs / 1,000,000 times = 567.231606000 ns
   // lower_bound:: 0.557699670 secs / 1,000,000 times = 557.699670000 ns
   // upper_bound:: 0.540888584 secs / 1,000,000 times = 540.888584000 ns
   // clear:      : 0.054685135 secs / 1,000,000 times =  54.685135000 ns
   // ----- random -----
   // emplace:    : 0.601261180 secs / 1,000,000 times = 601.261180000 ns
   // find:       : 0.576601145 secs / 1,000,000 times = 576.601145000 ns
   // lower_bound:: 0.562178143 secs / 1,000,000 times = 562.178143000 ns
   // upper_bound:: 0.555208289 secs / 1,000,000 times = 555.208289000 ns
   // clear:      : 0.133885606 secs / 1,000,000 times = 133.885606000 ns
   // 
   // ===== SortedVector / sequence =====
   // emplace:    : 0.398940040 secs / 1,000,000 times = 398.940040000 ns
   // find:       : 0.559192173 secs / 1,000,000 times = 559.192173000 ns
   // lower_bound:: 0.529487171 secs / 1,000,000 times = 529.487171000 ns
   // upper_bound:: 0.539333506 secs / 1,000,000 times = 539.333506000 ns
   // clear:      : 0.004536069 secs / 1,000,000 times =   4.536069000 ns
   // ----- random -----
   // emplace:: 2,114.961430451 secs / 1,000,000 times =   2.114961430 ms
   // find:       : 1.040423612 secs / 1,000,000 times =1040.423612    ns
   // lower_bound:: 0.999334112 secs / 1,000,000 times = 999.334112000 ns
   // upper_bound:: 0.932749620 secs / 1,000,000 times = 932.749620000 ns
   // clear:      : 0.004885941 secs / 1,000,000 times =   4.885941000 ns
   //using AuxSortedVector = AuxTest<SortedVector>;
   //if (iname == nullptr || strcmp(iname, "svect") == 0)
   //   AuxSortedVector::Benchmark("SortedVector");
}
