/// \file fon9/seed/FieldMaker.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_FieldMaker_hpp__
#define __fon9_seed_FieldMaker_hpp__
#include "fon9/seed/FieldString.hpp"
#include "fon9/seed/FieldChars.hpp"
#include "fon9/seed/FieldInt.hpp"
#include "fon9/seed/FieldBytes.hpp"
#include "fon9/seed/FieldDecimal.hpp"
#include "fon9/seed/FieldTimeStamp.hpp"
#include "fon9/seed/Tab.hpp"
#include "fon9/Outcome.hpp"

namespace fon9 { namespace seed {

/// \ingroup seed
// 避免使用 offsetof() 時, gcc 的善意提醒: offsetof within non-standard-layout type ‘...’ is undefined
#define fon9_OffsetOf(DerivedRaw, Member_) \
   static_cast<size_t>(reinterpret_cast<intptr_t>(&(reinterpret_cast<DerivedRaw*>(0x10000)->Member_)) \
                      - static_cast<intptr_t>(0x10000))

/// \ingroup seed
/// 計算: 透過 CastToRawPointer() 存取 Member_ 的偏移位置.
#define fon9_OffsetOfRawPointer(DerivedRaw, Member_) \
 ( static_cast<int32_t>(fon9_OffsetOf(DerivedRaw, Member_)) \
 - static_cast<int32_t>(reinterpret_cast<intptr_t>(fon9::seed::CastToRawPointer(reinterpret_cast<DerivedRaw*>(0x1000))) \
                       - static_cast<intptr_t>(0x1000)) )

/// \ingroup seed
/// 建立可存取 DerivedRaw::Member_ 的欄位.
/// 若 DerivedRaw::Member_ 為 const, 則建立出來的欄位不提供修改(StrToCell,PutNumber,SetNull,Copy)功能.
/// \code
///   using Pri = Decimal<uint64_t,6>;
///   using Qty = uint64_t;
///   struct PriQty {
///      Pri   Pri_;
///      Qty   Qty_;
///   };
///   struct Quote : public Raw {
///      PriQty   Bid_;
///      PriQty   Ask_;
///   };
///   ...
///   fon9_MakeField(Named{"BidPri"}, Quote, Bid_.Pri_);
/// \endcode
#define fon9_MakeField(named, DerivedRaw, Member_) \
   fon9::seed::MakeField(std::move(named), \
                        fon9_OffsetOfRawPointer(DerivedRaw, Member_), \
                        reinterpret_cast<DerivedRaw*>(0x1000)->Member_)

/// \ingroup seed
/// 不論 DerivedRaw::Member_ 是否為 const,
/// 建立出來的欄位一律不提供修改(StrToCell,PutNumber,SetNull,Copy)功能.
/// 只能取的欄位內容.
#define fon9_MakeField_const(named, DerivedRaw, Member_) \
   fon9::seed::MakeField(std::move(named), \
                        fon9_OffsetOfRawPointer(DerivedRaw, Member_), \
                        reinterpret_cast<const DerivedRaw*>(0x1000)->Member_)

/// \ingroup seed
/// 建立一個動態欄位.
/// - 格式: `Type FieldName|Title|Description\n`
///   - chSpl = '|'
///   - chTail = '\n'
/// - 解析到 chTail 或字串結束.
/// - FieldName, Title, Descrption: \ref DeserializeNamed()
/// - Type:
///    - Cn = char[n], 若 n==0 則為 Blob.Chars 欄位.
///    - Bn = byte[n], 若 n==0 則為 Blob.Bytes 欄位.
///    - Un[.s] = Unsigned, n=bytes(1,2,4,8), s=scale(小數位數)可為0
///    - Sn[.s] = Signed, n=bytes(1,2,4,8), s=scale(小數位數)可為0
///    - Unx = Unsigned, n=bytes(1,2,4,8), CellRevPrint()使用Hex輸出
///    - Snx = Signed, n=bytes(1,2,4,8), CellRevPrint()使用Hex輸出
///    - Ti = TimeInterval
///    - Ts = TimeStamp
/// - 例:
///    - C4    BrkNo |券商代號
///    - U4    IvacNo|投資人帳號|投資人帳號含檢查碼
///    - C1    Side  |買賣別|B=買進,S=賣出
///    - C1    OType |資券現|'0=現,1=資,2=券'     # 0=現股,1=代資,2=代券,3=自資,4=自券
///    - C6    Symbol|股票代號
///    - U4    Qty   |下單數量
///    - S4.2  Pri   |價格
///
/// \retval nullptr  格式有誤, fldcfg.begin() = 錯誤位置.
/// \retval !nullptr 成功建立一個欄位, fldcfg.begin() = fldcfg.end() 或 chTail 的下一個位置.
fon9_API FieldSP MakeField(StrView& fldcfg, char chSpl, char chTail);

/// \ingroup seed
/// 解析欄位設定, 建立動態欄位列表.
struct fon9_API FieldsMaker {
   /// 最後一次呼叫 MakeField() 時的開頭位置(已移除空白), 當返回錯誤時可得知「錯誤行」的位置.
   const char* LastLinePos_;

   /// 透過設定建立欄位列表.
   /// 如果欄位重複設定(名稱相同), 則:
   /// - 如果欄位型別不同, 則視為錯誤.
   /// - 欄位顯示文字、描述文字: 使用最後一個非空白的設定.
   ///
   /// \retval OpResult::value_format_error  格式錯誤
   /// \retval OpResult::key_exists          欄位已存在,但欄位型別不同.
   OpResult Parse(StrView& fldcfg, char chSpl, char chTail, Fields& fields);

   /// 有額外屬性的欄位設定方式: `{exProp} Type FieldName|Title|Description\n`
   /// exProp = 移除大括號後的字串.
   /// 預設不支援, 傳回: OpResult::not_supported_cmd
   virtual OpResult OnFieldExProperty(Field& fld, StrView exProp);
   
};

/// \ingroup seed
/// \ref FieldsMaker::Parse()
inline OpResult MakeFields(StrView& fldcfg, char chSpl, char chTail, Fields& fields) {
   return FieldsMaker{}.Parse(fldcfg, chSpl, chTail, fields);
}

/// \ingroup seed
/// 將 field 的設定(不含 Named) 加到 fldcfg 尾端.
/// 格式: `Type` 或 `(flags) Type` 尾端不含空白
fon9_API void AppendFieldConfig(std::string& fldcfg, const Field& field);
/// \ingroup seed
/// 將 fields 的設定(包含 Named) 加到 fldcfg 尾端.
/// 格式: `Type FieldName|Title|Description\n` 或 `(flags) Type FieldName|Title|Description\n`
///   - chSpl = '|'
///   - chTail = '\n'
fon9_API void AppendFieldsConfig(std::string& fldcfg, const Fields& fields, char chSpl, char chTail);
inline std::string MakeFieldsConfig(const Fields& fields, char chSpl, char chTail) {
   std::string fldcfg;
   AppendFieldsConfig(fldcfg, fields, chSpl, chTail);
   return fldcfg;
}

} } // namespaces
#endif//__fon9_seed_FieldMaker_hpp__
