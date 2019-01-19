// \file fon9/fmkt/FmktTypes.h
// \author fonwinz@gmail.com
#ifndef __fon9_fmkt_FmktTypes_h__
#define __fon9_fmkt_FmktTypes_h__
#include "fon9/sys/Config.h"
#include <stdint.h>

/// \ingroup fmkt
/// 商品的交易市場.
/// - 由系統決定市場別, 用於可直接連線的交易市場.
/// - 這裡沒有將所有可能的 market 都定義出來, 當有實際需求時可自行依下述方式增加.
/// - 底下以台灣市場為例:
///   - 如果系統有交易外匯, 則可增加:
///     `constexpr fon9::fmkt::TradingMarket  f9fmkt_TradingMarket_Forex = static_cast<fon9::fmkt::TradingMarket>('x');`
///   - 如果系統可直接連上 SGX, 則可增加:
///     `constexpr fon9::fmkt::TradingMarket  f9fmkt_TradingMarket_SGX = static_cast<fon9::fmkt::TradingMarket>('g');`
///   - 台灣大部分券商的對外(國外)期權, 可視為「一個」市場:
///     `constexpr fon9::fmkt::TradingMarket  f9fmkt_TradingMarket_FF = static_cast<fon9::fmkt::TradingMarket>('f');`
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

/// \ingroup fmkt
/// 下單要求狀態.
enum f9fmkt_TradingRequestSt   fon9_ENUM_underlying(uint8_t) {
   f9fmkt_TradingRequestSt_Init = 0,

   /// - 當檢查方式需要他方協助, 無法立即得到結果, 則在檢查前設定此狀態.
   /// - 如果檢查方式可立即完成:「失敗拒絕下單 or 檢查通過繼續下一步驟」, 則檢查前不必設定此狀態.
   f9fmkt_TradingRequestSt_Checking = 0x1c,

   f9fmkt_TradingRequestSt_Queuing = 0x20,

   /// 在呼叫 io.Send() 之前設定的狀態.
   /// 您可以自行決定要在 io.Send() 之前 or 之後 or both, 設定送出狀態.
   f9fmkt_TradingRequestSt_Sending = 0x30,
   /// 在呼叫 io.Send() 之後設定的狀態.
   /// 如果要在送出後設定狀態: 則要小心考慮在 io.Send() 之後, 設定 Sent 之前, 就已收到送出的結果.
   f9fmkt_TradingRequestSt_Sent = 0x38,

   /// 下單要求流程已結束.
   f9fmkt_TradingRequestSt_Done = 0xd0,
   /// 不明原因結束, 無法確定此要求是否成功.
   /// 例如: 要求送出後斷線.
   f9fmkt_TradingRequestSt_Broken = 0xdb,

   /// 下單要求因「線路狀況」拒絕. 尚未送給交易所.
   /// 例如: 無可用線路.
   f9fmkt_TradingRequestSt_LineRejected = 0xe3,
   /// 下單要求因「風控檢查」拒絕. 尚未送給交易所.
   f9fmkt_TradingRequestSt_CheckingRejected = 0xec,
   /// 下單要求因「內部其他原因」拒絕. 尚未送給交易所.
   f9fmkt_TradingRequestSt_InternalRejected = 0xed,
   /// 下單要求被「交易所」拒絕.
   f9fmkt_TradingRequestSt_ExchangeRejected = 0xee,

   /// 新單要求: 尚未收到新單回報, 但先收到成交回報.
   f9fmkt_TradingRequestSt_FilledBeforeNew = 0xf1,
   /// 已收到交易所的確認回報.
   f9fmkt_TradingRequestSt_ExchangeAccepted = 0xf8,

   /// 下單要求流程已結束, 但後續又有變動.
   /// 例如: FIX 的 ExecType=D(Restated).
   f9fmkt_TradingRequestSt_Restated = 0xfc,
};

enum f9fmkt_Side fon9_ENUM_underlying(char) {
   f9fmkt_Side_Unknown = 0,
   f9fmkt_Side_Buy = 'B',
   f9fmkt_Side_Sell = 'S',
};

#endif//__fon9_fmkt_FmktTypes_h__
