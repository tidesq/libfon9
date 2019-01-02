// \file fon9/Symb.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_Symb_hpp__
#define __fon9_fmkt_Symb_hpp__
#include "fon9/fmkt/FmktTypes.h"
#include "fon9/CharVector.hpp"
#include "fon9/intrusive_ref_counter.hpp"
#include "fon9/Trie.hpp"
#include "fon9/seed/Tab.hpp"

#include <unordered_map>

namespace fon9 { namespace fmkt {

/// \ingroup fmkt
/// 為了讓 SymbTree::PodOp 裡面的 seed::SimpleRawRd{*symbdata} 可以正確運作,
/// SymbData 的衍生者, 必須將 「SymbData 基底」放在第一位,
/// 請參考 class Symb; class SymbRef;
class fon9_API SymbData {
   fon9_NON_COPY_NON_MOVE(SymbData);
public:
   SymbData() = default;
   virtual ~SymbData();
};

/// \ingroup fmkt
/// 可擴充的商品資料.
/// - 配合 seed::LayoutDy 或 LayoutN 機制擴充, 例如:
///   - 參考價.
///   - 買賣報價(量).
///   - 成交價量.
class fon9_API Symb : public SymbData, public intrusive_ref_counter<Symb> {
   fon9_NON_COPY_NON_MOVE(Symb);
public:
   /// 一張的單位數(股數)
   uint32_t ShUnit_{0};
   /// 台灣期交所的流程群組代碼.
   uint16_t FlowGroup_{0};
   /// 交易市場.
   f9fmkt_TradingMarket TradingMarket_{f9fmkt_TradingMarket_Unknown};
   char                 Filler5_____[5];

   /// 系統自訂的商品Id, 不一定等於交易所的商品Id.
   /// 交易送出、行情解析時, 再依自訂規則轉換.
   const CharVector  SymbId_;

   Symb(const StrView& symbid) : SymbId_{symbid} {
   }

   virtual SymbData* GetSymbData(int tabid);
   virtual SymbData* FetchSymbData(int tabid);

   static seed::Fields MakeFields();
};
using SymbSP = intrusive_ptr<Symb>;

//--------------------------------------------------------------------------//

using SymbHashMap = std::unordered_map<StrView, SymbSP>;
inline StrView GetSymbId(const std::pair<StrView, SymbSP>& v) {
   return v.first;
}
inline Symb& GetSymbValue(const std::pair<StrView, SymbSP>& v) {
   return *v.second;
}
inline void ResetSymbValue(std::pair<StrView, SymbSP>& v, SymbSP symb) {
   v.second = std::move(symb);
}
inline void ResetSymbValue(SymbHashMap::value_type& v, SymbSP symb) {
   v.second = std::move(symb);
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
inline Symb& GetSymbValue(const SymbTrieMap::value_type& v) {
   return *v.value();
}
inline void ResetSymbValue(SymbTrieMap::value_type& v, SymbSP symb) {
   v.value() = std::move(symb);
}

//--------------------------------------------------------------------------//

/// \ingroup fmkt
/// - 根據 Symb_UT.cpp 的測試結果, std::unordered_map 比 std::map、fon9::Trie
///   更適合用來處理「全市場」的「商品資料表」, 因為資料筆數較多, 且商品Id散亂(不適合使用Trie)。
/// - 但是如果是「個別帳戶」的「商品庫存」(個別的資料筆數較少, 但帳戶可能很多),
///   則仍需其他驗證, 才能決定用哪種容器: 「帳戶庫存表」應考慮使用較少記憶體的容器。
using SymbMap = SymbHashMap;
//using SymbMap = SymbTrieMap;

} } // namespaces
#endif//__fon9_fmkt_Symb_hpp__
