// \file fon9/Symb.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_Symb_hpp__
#define __fon9_fmkt_Symb_hpp__
#include "fon9/CharVector.hpp"
#include "fon9/intrusive_ref_counter.hpp"
#include "fon9/Trie.hpp"
#include <unordered_map>

namespace fon9 { namespace fmkt {

class fon9_API SymbData {
   fon9_NON_COPY_NON_MOVE(SymbData);
public:
   SymbData() = default;
   virtual ~SymbData();
};

/// \ingroup fmkt
/// 這裡沒有將所有可能的 market 都定義出來,
/// 僅提供範例, 若有其他擴充可使用:
/// `constexpr fon9::fmkt::SymbMarket  SymbMarket_Fx = static_cast<fon9::fmkt::SymbMarket>('f');`
enum SymbMarket : char {
   SymbMarket_Unknown = '\0',
   SymbMarket_TwSEC = 'T',
   SymbMarket_TwOTC = 'O',
   SymbMarket_TwFex = 'F',
};
constexpr fon9::fmkt::SymbMarket  SymbMarket_TwEmg = static_cast<fon9::fmkt::SymbMarket>('E'); 

/// \ingroup fmkt
/// 可擴充的商品資料.
/// - 配合 seed::LayoutDy 或 LayoutN 機制擴充, 例如:
///   - 參考價.
///   - 買賣報價(量).
///   - 成交價量.
class Symb : public SymbData, public intrusive_ref_counter<Symb> {
   fon9_NON_COPY_NON_MOVE(Symb);
   // 使用 LayoutDy 動態建立所需的資料.
   // using SymbDataSP = std::unique_ptr<SymbData>;
   // using ExDatas = std::vector<SymbDataSP>;
   // ExDatas  ExDatas_;
public:
   SymbMarket        Market_{SymbMarket_Unknown};
   const CharVector  SymbId_;

   Symb(const StrView& id) : SymbId_{id} {
   }
};
using SymbSP = intrusive_ptr<Symb>;

//--------------------------------------------------------------------------//

using SymbHashMap = std::unordered_map<StrView, SymbSP>;
inline StrView GetSymbId(const std::pair<StrView, SymbSP>& v) {
   return v.first;
}
inline Symb& GetSymbRef(const std::pair<StrView, SymbSP>& v) {
   return *v.second;
}

//--------------------------------------------------------------------------//

/// 僅允許 ASCII 裡面的: 0x20(space)..0x5f(_) 當作 key, 不支援小寫及「 ` { | } ~ 」
/// 如果有超過範圍的字元, 則視為相同, 放在該層的最後. 例如: "A12|4" == "A12~4".
/// 無效字元 ToKey() 一律傳回 '~'
struct TrieSymbKey {
   using LvKeyT = char;
   using LvIndexT = fon9::byte;
   using key_type = fon9::StrView;
   static constexpr LvIndexT MaxIndex() {
      return 0x5f - 0x20 + 1;
   }
   fon9_GCC_WARN_DISABLE("-Wconversion"); // 抑制 val -= 0x20; idx += 0x20; 造成的 warning.
   static LvIndexT ToIndex(LvKeyT val) {
      return static_cast<LvIndexT>(val -= 0x20) > MaxIndex()
         ? MaxIndex() : static_cast<LvIndexT>(val);
   }
   static LvKeyT ToKey(LvIndexT idx) {
      return (idx += 0x20) > 0x5f ? '~' : static_cast<LvKeyT>(idx);
   }
   fon9_GCC_WARN_POP;
};
using SymbTrieMap = Trie<TrieSymbKey, SymbSP>;
inline StrView GetSymbId(const SymbTrieMap::value_type& v) {
   return ToStrView(v.value()->SymbId_);
}
inline Symb& GetSymbRef(const SymbTrieMap::value_type& v) {
   return *v.value();
}

//--------------------------------------------------------------------------//

/// \ingroup fmkt
/// - 根據 Symb_UT.cpp 的測試結果, std::unordered_map 比 std::map、fon9::Trie
///   更適合用來處理「全市場」的「商品資料表」, 因為資料筆數較多, 且商品Id散亂(不適合使用Trie)。
/// - 但是如果是「個別帳戶」的「商品庫存」(個別的資料筆數較少,但帳戶可能很多),
///   則仍需其他驗證, 才能決定用哪種容器: 「帳戶庫存表」應考慮使用較少記憶體的容器。
//using SymbMap = SymbHashMap;
using SymbMap = SymbTrieMap;

} } // namespaces
#endif//__fon9_fmkt_Symb_hpp__
