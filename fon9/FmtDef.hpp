/// \file fon9/FmtDef.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_FmtDef_hpp__
#define __fon9_FmtDef_hpp__
#include "fon9/StrView.hpp"
#include "fon9/Utility.hpp"

namespace fon9 {

/// \ingroup AlNum
/// 格式設計: 盡量符合 printf() 的 format 的定義.
enum class FmtFlag : uint32_t {
   /// 當要輸出的數字為 0 時:
   /// 輸出指定寬度 `FmtDef::Width_` 個空白字元, 若寬度為 0 則不輸出任何字元.
   Hide0 = 0x0001,
   /// 當有指定寬度 `FmtDef::Width_ > 0` 時: 靠左, 如果沒設定此旗標, 則預設為指定寬度靠右.
   /// 若有設定此旗標, 則不理會 FmtFlag::IntPad0
   LeftJustify = 0x0002,

   /// 當「有指定寬度」 且 「沒指定 FmtFlag::LeftJustify」 時, 則此旗標有效:
   /// - 整數部分左方補 '0'
   /// - 未設定此旗標則預設為補空白(' ')
   /// - 當此旗標有效設定時, 則不理會 FmtFlag::IntHide0
   IntPad0 = 0x0004,
   /// 若整數部分為0則不顯示, 例如: 數字0.123, 顯示「.123」.
   /// 當 FmtFlag::IntPad0 有效時, 則不理會此旗標.
   IntHide0 = 0x0008,

   /// 取出 [正負號處理旗標] 的遮罩: 0111 0000.
   /// 預設正負號處理方式: 正號不佔位、不顯示, 負數才顯示「-」.
   MaskSign = 0x0070,
   /// [正負號] 固定占用一位: 若 >=0 顯示空白, <0 顯示「-」.
   ShowSign = 0x0020,
   /// [正負號] 固定占用一位: 若 >=0 顯示「+」, <0 顯示「-」.
   ShowPlus0 = 0x0040,

   /// 指定進位基底時(BaseHex,BaseOct), 如果輸出的數字非0, 則加上前綴:
   /// - BaseHex: "0x"
   /// - BaseHEX: "0X"
   /// - BaseOct: "0"
   ShowPrefix = 0x0100,
   /// 浮點數(fon9::Decimal) 輸出時, 若小數部分為0, 強制加上小數點.
   /// 若有設定 (FmtFlag::NoDecimalPoint 及 FmtFlag::HasPrecision), 則不理會此旗標.
   ForceDecimalPoint = 0x0100,
   /// 浮點數(fon9::Decimal) 輸出時, 若有指定 FmtFlag::HasPrecision, 則強制 **移除** 小數點.
   /// 此時不理會 FmtFlag::ForceDecimalPoint 旗標.
   NoDecimalPoint = 0x0200,

   /// 當有設定 ".precision" 時, 會設定此旗標.
   HasPrecision = 0x0400,

   /// 整數部分加上千位分隔.
   /// 如果有用到 FmtFlag::IntPad0, 則補'0'的部分不考慮分隔.
   /// 有些國家地區, 小數不是「.」, 千位分隔不是「,」, 也不一定是[每1千]分隔!
   /// 這些由 NumPunct 來控制.
   ShowIntSep = 0x0800,

   MaskBase = 0x7000,
   /// 當使用整數輸出時, 使用16進位(0123456789abcdef)
   BaseHex = 0x1000,
   /// 當使用整數輸出時, 使用16進位(uppercase: 0123456789ABCDEF)
   BaseHEX = 0x2000,
   /// 當使用整數輸出時, 使用8進位.
   BaseOct = 0x3000,
   /// 當使用整數輸出時, 使用2進位.
   BaseBin = 0x4000,
};
fon9_ENABLE_ENUM_BITWISE_OP(FmtFlag);

/// \ingroup AlNum
/// 使用字串設定 FmtFlag 時, 字元代表的含意.
/// 參考 printf() format 的 flags 設計.
enum class FmtChar : char {
   LeftJustify = '-',
   ShowPlus0 = '+',
   ShowSign = ' ',
   IntPad0 = '0',
   ShowPrefix = '#',
   ForceDecimalPoint = '#',

   ShowIntSep = ',',
};

struct fon9_API FmtDef {
   FmtFlag  Flags_{};

   using WidthType = uint16_t;
   /// 最少輸出總寬度: 0 = don't care; else 說明如下:
   /// - 若實際輸出寬度超過此值, 則不理會此處設定, 也就是說輸出不會因此值過小而被截斷.
   /// - 若實際輸出寬度小於此值, 則:
   ///   - 有設定 FmtFlag::LeftJustify 則右方補空白
   ///   - 沒設定 FmtFlag::LeftJustify
   ///     - 為數字輸出且有設定 FmtFlag::IntPad0 則: 左方補 '0'
   ///     - 否則左方補空白
   WidthType Width_{0};
   /// 精確度位數: 0 = don't care; else 說明如下:
   /// - 一般整數: 最小數字輸出位數, 若數字輸出位數小於此值, 則於前方補 '0'
   ///   - 此時不理會 FmtFlag::IntPad0 旗標.
   /// - 浮點數: 小數點後的數字寬度(不含小數點), 不足部分尾端補 '0'
   ///   - 例: `1.23`; Precision_=4; 輸出為: "1.2300"
   /// - 字串: 最多輸出的字元數量, 若超過此值, 則截斷, 然後再參考 Width_ 輸出.
   ///   - 實際做法: 輸出的 byte 數量.
   ///   - 由於採用 UTF8, 所以一個字可能占用多個 bytes,
   ///     此時會將輸出切齊 UTF8 字元, 但「畫面輸出」字數, 可能不會符合此值.
   ///   - 例: u8str = "Hello 您好" => "\x48\x65\x6C\x6C\x6F\x20" "\xE6\x82\xA8" "\xE5\xA5\xBD"
   ///     - Precision_=10: "\x48\x65\x6C\x6C\x6F\x20" "\xE6\x82\xA8" "\xE5"
   ///     - 實際的輸出為:   "\x48\x65\x6C\x6C\x6F\x20" "\xE6\x82\xA8"
   ///       - 因為第 u8str[9], 為不完整的 UTF8 字, 所以整個字拿掉.
   ///       - 所以共輸出 9 bytes
   ///     - 畫面輸出為: "Hello 您" 共 7 個字.
   WidthType Precision_{0};

   /// 參考 printf() format 的設計: `[flags][width][.precision]`
   /// - 若有提供 `.precision` 則會設定 FmtFlag::HasPrecision
   /// - 若有提供 `_precision` 則會設定 FmtFlag::NoDecimalPoint
   explicit FmtDef(StrView fmtstr);

   FmtDef() = default;

   /// 解析 width[.precision]
   void ParseWidth(const char* pcur, const char* pend);
};

template <typename ValueT>
struct FmtSelector {
   using FmtType = FmtDef;
};

template <typename ValueT>
using AutoFmt = typename FmtSelector<ValueT>::FmtType;

} // namespace
#endif//__fon9_FmtDef_hpp__
