/// \file fon9/seed/Tab.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_Tab_hpp__
#define __fon9_seed_Tab_hpp__
#include "fon9/seed/Field.hpp"
#include "fon9/seed/Layout.hpp"

namespace fon9 { namespace seed {

/// \ingroup seed
/// 這裡的旗標只是在取得 layout 時, 提供操作的提示.
/// 還需要配合 TreeOp, PodOp 的實作.
enum TabFlag {
   /// 支援 PodOp::BeginWrite(); 修改欄位內容.
   Writable = 0x01,

   // 確定每個 seed 都有 sapling.
   /// 但不保證 sapling 會有資料,
   /// 例如: 投資帳號確定都有庫存表, 但不是每個帳號都有庫存.
   HasSapling = 0x02,
   /// 確定每個 seed 都沒有 sapling.
   /// 如果沒有 NoSapling, 也沒有 HasSapling,
   /// 則表示由 PodOp::GetSapling(); PodOp::MakeSapling(); 決定是否有 sapling.
   NoSapling = 0x04,

   /// 支援 PodOp::OnSeedCommand(); 執行指令, 且每個 Seed 都使用相同的指令集.
   /// 例如: /MaAuth/UserSha256/ 底下的每個 user 都支援相同的指令.
   HasSameCommandsSet = 0x10,
   /// 確定不支援 PodOp::OnSeedCommand(); 執行指令.
   /// 如果沒有 HasSameCommandsSet, 也沒有 HasSameCommandsSet,
   /// 則表示由各個 Seed 自行決定是否支援 OnSeedCommand();
   NoSeedCommand = 0x20,

   /// 需使用「套用」方式處理資料異動。
   /// - 修改中的資料會放在另一個 mtable.
   /// - 有 Write 權限者可以編修套用前的 mtable.
   /// - 但需有 Apply 權限, 才能按下 [套用]. 也就是編修與套用可以是不同 user.
   /// - 修改 mtable 前, 會先保留 [原資料].
   /// - 修改時會比對有異動的地方(增、刪、改), 讓修改者(或套用者)查看異動處.
   /// - 套用時, 檢查先前保留的 [原資料] 是否正確(沒有變動過), 正確才允許 [套用].
   /// - 若 [原資料] 有變動過, 則需再次取得 [原資料] 比對, 然後再次 [套用].
   NeedsApply = 0x80,
};
fon9_ENABLE_ENUM_BITWISE_OP(TabFlag);

/// \ingroup seed
/// 欄位列表.
class fon9_API Fields {
   Fields(const Fields&) = delete;
   Fields& operator=(const Fields&) = delete;

   using NamedFields = NamedIxMapNoRemove<FieldSP>;
   NamedFields Fields_;
   friend uint32_t CalcDyMemSize(Fields& fields, Tab* tab, Field* keyfld);
public:
   Fields() = default;
   Fields(Fields&& r) : Fields_(std::move(r.Fields_)) {
   }

   size_t size() const {
      return this->Fields_.size();
   }

   /// \retval true  成功將 fld 加入
   /// \retval false 加入失敗: fld->Name_ 重複, 或 (fld->GetIndex() >= 0)
   bool Add(FieldSP&& fld) {
      return this->Fields_.Add(std::move(fld));
   }
   Field* Get(StrView name) {
      return this->Fields_.Get(name);
   }

   const Field* Get(size_t index) const {
      return this->Fields_.Get(index);
   }
   const Field* Get(StrView name) const {
      return this->Fields_.Get(name);
   }
   size_t GetAll(std::vector<const Field*>& flds) const {
      return this->Fields_.GetAll(*reinterpret_cast<std::vector<Field*>*>(&flds));
   }
};

/// \ingroup seed
/// 用來描述 Seed 的外觀:
/// - 透過 Fields 描述資料.
/// - 若有 DyMem, 則必需透過 MakeDyMemRaw() 建立資料.
class fon9_API Tab : public NamedIx, public intrusive_ref_counter<Tab> {
   fon9_NON_COPY_NON_MOVE(Tab);
public:
   const TabFlag  Flags_;

   /// Blob欄位的數量.
   const uint32_t DyBlobCount_;
   /// 若 Tab 建立的資料有動態分配的欄位,
   /// 則此處包含了需要額外分配的記憶體數量.
   /// 所以建立 Raw 物件必須透過: MakeDyMemRaw<MyRawType>(Tab, rawCtorParams...);
   const uint32_t DyMemSize_;

   /// 此 Tab 建立的 Seed(Raw) 所擁有的欄位.
   const Fields   Fields_;
   /// 若這裡是有效的 Layout, 則表示 this tab 建立的 Seed 全都包含相同 Layout.
   /// Tab 是否包含相同 Layout, 必須在 Tab 建構時就決定.
   const LayoutSP SaplingLayout_;

   /// 必須在建構前就先定義好欄位,
   /// Tab 運行中不允許增減欄位.
   /// 若有 DyMem 欄位, 建構時會調整 field 的 offset.
   /// 如果 keyfld 是存放在此 tab 裡面的 DyMem 欄位, 則必須在此提供, 用來計算 DyBlobCount_, DyMemSize_;
   Tab(const Named& named, Fields&& fields, LayoutSP saplingLayout = LayoutSP{}, TabFlag flags = TabFlag{}, Field* keyfld = nullptr);
   Tab(const Named& named, Fields&& fields, TabFlag flags, Field* keyfld = nullptr)
      : Tab(named, std::move(fields), LayoutSP{}, flags, keyfld) {
   }

   virtual ~Tab();
};

//--------------------------------------------------------------------------//

/// \ingroup seed
/// 協助建構時從 fields 取出欄位, 自動轉成 fldDataMember 的欄位型別.
/// 例如:
/// \code
///   struct Owner {
///      fon9_NON_COPY_NON_MOVE(Owner);
///      const fon9::seed::FieldChars* const FldFromIp_;
///      Owner(const fon9::seed::Fields& fields)
///         : FldFromIp_{fon9::seed::GetTypedField(fields, "FromIp", &Owner::FldFromIp_)} {
///      }
///   };
/// \endcode
template <class DerivedField, class Owner>
DerivedField* GetTypedField(const Fields& fields, StrView fieldName, DerivedField* const Owner::*fldDataMember) {
   (void)fldDataMember;
   return dynamic_cast<DerivedField*>(fields.Get(fieldName));
}

/// \ingroup seed
/// 協助建構時取出必要的 Field, 例如:
/// \code
///   struct Owner {
///      fon9_NON_COPY_NON_MOVE(Owner);
///      const fon9::seed::FieldChars* const FldFromIp_;
///      Owner(const fon9::seed::Fields& fields)
///         : fon9_CTOR_GetTypedField4(fields, "FromIp", Owner, FldFromIp_) {
///      }
///   };
/// \endcode
#define fon9_CTOR_GetTypedField4(fields, OwnerT, FieldName, DataMember) \
DataMember{GetTypedField(fields, FieldName, &OwnerT::DataMember)}

/// \ingroup seed
/// 協助建構時取出必要的 Field, 例如:
/// \code
///   struct Owner {
///      fon9_NON_COPY_NON_MOVE(Owner);
///      const fon9::seed::FieldChars* const FldFromIp_;
///      Owner(const fon9::seed::Fields& fields)
///         : fon9_CTOR_GetTypedField3(fields, FromIp, Owner) {
///      }
///   };
/// \endcode
#define fon9_CTOR_GetTypedField3(fields, OwnerT, FieldName) \
fon9_CTOR_GetTypedField4(fields, OwnerT, #FieldName, Fld##FieldName##_)

} } // namespaces
#endif//__fon9_seed_Tab_hpp__
