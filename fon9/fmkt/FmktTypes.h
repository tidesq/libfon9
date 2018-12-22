// \file fon9/FmktTypes.h
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_FmktTypes_h__
#define __fon9_fmkt_FmktTypes_h__
#include "fon9/sys/Config.h"

/// \ingroup fmkt
/// 商品的交易市場.
/// - 由系統決定市場別, 用於可直接連線的交易市場.
/// - 這裡沒有將所有可能的 market 都定義出來, 當有實際需求時可自行依下述方式增加.
/// - 底下以台灣市場為例:
/// - TwSEC, TwOTC, TwFex
/// - 如果系統有交易外匯, 則可增加:
///   `constexpr fon9::fmkt::TradingMarket  TradingMarket_Forex = static_cast<fon9::fmkt::TradingMarket>('x');`
/// - 如果系統可直接連上 SGX, 則可增加:
///   `constexpr fon9::fmkt::TradingMarket  TradingMarket_SGX = static_cast<fon9::fmkt::TradingMarket>('g');`
/// - 台灣大部分券商的對外(國外)期權, 可視為「一個」市場:
///   `constexpr fon9::fmkt::TradingMarket  TradingMarket_FF = static_cast<fon9::fmkt::TradingMarket>('f');`
enum f9fmkt_TradingMarket   fon9_ENUM_underlying(char) {
   f9fmkt_TradingMarket_Unknown = '\0',
   /// 台灣上市(集中交易市場)
   f9fmkt_TradingMarket_TwSEC = 'T',
   /// 台灣上櫃(櫃檯買賣中心)
   f9fmkt_TradingMarket_TwOTC = 'O',
   /// 台灣興櫃.
   f9fmkt_TradingMarket_TwEmg = 'E',
   /// 台灣期交所.
   f9fmkt_TradingMarket_TwFex = 'F',
};

/// \ingroup fmkt
/// 交易時段代碼.
/// - FIX 4.x TradingSessionID(#336): 並沒有明確的定義, 由雙邊自訂.
/// - FIX 5.0 TradingSessionID(#336): http://fixwiki.org/fixwiki/TradingSessionID
///   '1'=Day; '2'=HalfDay; '3'=Morning; '4'=Afternoon; '5'=Evening; '6'=AfterHours; '7'=Holiday;
/// - 台灣期交所 FIX 的 Trading Session Status Message 定義 TradingSessionID=流程群組代碼.
/// - 台灣證交所 FIX 沒有 Trading Session Status Message,
///   但要求提供 TargetSubID(#57)=[TMP協定的 AP-CODE]:
///   Trading session: Required for New / Replace / Cancel / Query / Execution
///   '0'=Regular Trading; '7'=FixedPrice; '2'=OddLot.
enum f9fmkt_TradingSessionId   fon9_ENUM_underlying(char) {
   f9fmkt_TradingSessionId_Unknown = 0,
   /// 一般(日盤).
   f9fmkt_TradingSessionId_Normal = 'N',
   /// 零股.
   f9fmkt_TradingSessionId_OddLot = 'O',
   /// 盤後定價.
   f9fmkt_TradingSessionId_FixedPrice = 'F',
   /// 盤後交易(夜盤).
   f9fmkt_TradingSessionId_AfterHour = '6',

   /// 提供給陣列使用, 例如:
   /// using SessionMap = std::array<SessionRec, TradingSessionId_MaxIndex + 1u>;
   f9fmkt_TradingSessionId_MaxIndex = 'z',
};

#endif//__fon9_fmkt_FmktTypes_h__
