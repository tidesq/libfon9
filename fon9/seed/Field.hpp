/// \file fon9/seed/Field.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_Field_hpp__
#define __fon9_seed_Field_hpp__
#include "fon9/seed/SeedBase.hpp"
#include "fon9/buffer/RevBuffer.hpp"
#include "fon9/ToStr.hpp"
#include "fon9/NamedIx.hpp"

namespace fon9 { namespace seed {

/// \ingroup seed
/// 呼叫 Field::GetNumber(); Field::PutNumber(); 時, 使用的數字型別.
using FieldNumberT = intmax_t;

/// \ingroup seed
/// 欄位的資料來源: 來自 data member 或 動態分配.
/// >= DyMem 則表示 [動態記憶體欄位: 必須透過 MakeDyMemRaw() 建構, 在 MakeDyMemRaw() 裡面會額外分配記憶體].
enum class FieldSource : uint8_t {
   /// Field 存取的是 data member.
   DataMember,

   /// 透過 RawRd::UserDefine_ 存取自訂欄位.
   UserDefine,

   /// 欄位內容儲存在「透過 MakeDyMemRaw() 建構時，額外動態分配的」記憶體之中.
   DyMem,
   /// 動態分配的Blob欄位.
   DyBlob,
};

/// \ingroup seed
/// 欄位儲存的資料類型.
enum class FieldType : uint8_t {
   /// 類型不明, 或自訂欄位.
   Unknown,

   /// bytes 陣列.
   /// 顯示字串 = 需要額外轉換(例: HEX, Base64...)
   Bytes,

   /// 字元串(尾端不一定會有EOS).
   /// 顯示字串 = 直接使用內含字元, 直到EOS or 最大長度.
   Chars,

   /// 整數欄位.
   Integer = 10,

   /// 固定小數位欄位: fon9::Decimal
   Decimal,

   /// 時間戳.
   TimeStamp,

   /// 時間間隔.
   TimeInterval,
};
inline bool IsFieldTypeNumber(FieldType t) {
   return t >= FieldType::Integer;
}

/// \ingroup seed
enum class FieldFlag {
   /// 如果需要在設計階段, 就決定要禁止透過 Field 修改欄位,
   /// 則應使用 FieldConst<> 來建立欄位物件.
   /// 此旗標僅提供給與 UI 相關的參考.
   /// e.g. 與UI通訊的模組, 如果發現對方想要修改 Readonly 欄位, 應寫入警告 log, 並強制斷線.
   Readonly = 0x01,
   /// 在處理顯示相關作業時, 藉由判斷此旗標決定是否需要顯示欄位內容.
   /// 若不需要顯示, 則可能不會傳送給 UI 端.
   Hide = 0x02,
};
fon9_ENABLE_ENUM_BITWISE_OP(FieldFlag);

enum class FieldFlagChar : char {
   Readonly = 'r',
   Hide = 'h',
};


fon9_WARN_DISABLE_PADDING;
/// \ingroup seed
/// 透過 Field 可用來存取 Raw instance 裡面的一個欄位.
class fon9_API Field : public NamedIx {
   fon9_NON_COPY_NON_MOVE(Field);
public:
   Field(Named&& named, FieldType type, FieldSource src, int32_t offset, size_t size, DecScaleT decScale)
      : NamedIx{std::move(named)}
      , Offset_{offset}
      , Size_{static_cast<uint32_t>(size)}
      , Source_{src}
      , Type_{type}
      , DecScale_{decScale} {
      assert(size == this->Size_); // 檢查 size_t => uint32_t 是否被截短了?
   }
   virtual ~Field();

   /// 在 Tab 建構時設定,
   /// 然後在使用 Field 存取 Raw 內容時檢查(開發階段):
   /// - 避免 Tab A 建立的 Raw 卻透過 Tab B 的欄位來存取.
   /// - 由於 tab->Fields_->Get() 取出都是 const Field*,
   /// - 所以這裡不用加上 Tab* const, 正常使用時也無法變動.
   Tab*        Tab_{};
   /// 從 Raw 取出資料的 offset.
   /// 在 Tab 建構時, 若為 DyMem,DyBlob... 欄位, 則會自動計算此欄位.
   /// 在使用 data member 的情況, Offset_ 有可能為負值, 例:
   /// - class MySeed : public Base1, public Raw {};
   /// - 如果使用 Base1 的 data member, 則 Offset_ 為負值!
   int32_t     Offset_;
   /// 欄位資料的大小.
   uint32_t    Size_;
   FieldSource Source_;
   FieldType   Type_;
   FieldFlag   Flags_{};
   DecScaleT   DecScale_;

   /// 取得型別字串: 參考 MakeField() 建立.
   /// 傳回值不一定會填入 buf.
   virtual StrView GetTypeId(NumOutBuf& buf) const = 0;

   /// 取出 rd 的資料, 根據 fmt 指定的格式, 輸出字串填入 out.
   virtual void CellRevPrint(const RawRd& rd, StrView fmt, RevBuffer& out) const = 0;

   /// 把 value(文字字串) 轉入 wr 內部儲存形式。
   /// \retval no_error             成功轉入.
   /// \retval not_supported_write  e.g. const欄位不支援填入資料.
   /// \retval value_format_error   value格式有誤.
   /// \retval value_overflow
   /// \retval value_underflow
   virtual OpResult StrToCell(const RawWr& wr, StrView value) const = 0;

   /// 取出欄位數字內容, 可能觸發文字轉數字(由衍生者決定).
   /// - 如果實際的欄位內容是 123;   outDecScale = 1; 則傳回 1230
   /// - 如果實際的欄位內容是 12.34; outDecScale = 1; 則傳回 123
   virtual FieldNumberT GetNumber(const RawRd& rd, DecScaleT outDecScale, FieldNumberT nullValue) const = 0;

   /// 把數字轉成正確的型別資料後, 填入 wr 的欄位, 可能觸發數字轉文字.
   /// num = 123; decScale = 1; 表示要填入的數字是 12.3
   virtual OpResult PutNumber(const RawWr& wr, FieldNumberT num, DecScaleT decScale) const = 0;

   /// 設定 wr 的欄位內容為空值.
   /// 如何填入空值由衍生者決定.
   virtual OpResult SetNull(const RawWr& wr) const;
   /// 判斷 rd 的欄位內容是否為空.
   /// 預設傳回false.
   virtual bool IsNull(const RawRd& rd) const;

   /// 複製欄位內容.
   virtual OpResult Copy(const RawWr& wr, const RawRd& rd) const = 0;
};
fon9_WARN_POP;

/// \ingroup seed
/// 針對 const data member Field 的額外包裝.
/// 任何修改 Raw 的方法(例: StrToCell(), SetNull()...): 不做任何事 (可考慮呼叫 assert() 或記錄 log).
template <class BaseFieldT>
class FieldConst : public BaseFieldT {
   fon9_NON_COPY_NON_MOVE(FieldConst);
   typedef BaseFieldT   base;
public:
   template <class... ArgsT>
   FieldConst(ArgsT&&... args) : base{std::forward<ArgsT>(args)...} {
      this->Flags_ |= FieldFlag::Readonly;
   }

   virtual OpResult StrToCell(const RawWr& wr, StrView value) const override {
      (void)wr; (void)value;
      return OpResult::not_supported_write;
   }
   virtual OpResult SetNull(const RawWr& wr) const override {
      (void)wr;
      return OpResult::not_supported_write;
   }
   virtual OpResult PutNumber(const RawWr& wr, FieldNumberT num, DecScaleT decScale) const override {
      (void)wr; (void)num; (void)decScale;
      return OpResult::not_supported_write;
   }
   virtual OpResult Copy(const RawWr& wr, const RawRd& rd) const override {
      (void)wr; (void)rd;
      return OpResult::not_supported_write;
   }
};

} } // namespaces
#endif//__fon9_seed_Field_hpp__
