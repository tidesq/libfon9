/// \file fon9/seed/Tab.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_Tab_hpp__
#define __fon9_seed_Tab_hpp__
#include "fon9/seed/Field.hpp"
#include "fon9/seed/Layout.hpp"

namespace fon9 { namespace seed {

/// \ingroup seed
/// 欄位列表.
class fon9_API Fields {
   Fields(const Fields&) = delete;
   Fields& operator=(const Fields&) = delete;

   using NamedFields = NamedIxMapNoRemove<FieldSP>;
   NamedFields Fields_;
   friend uint32_t CalcDyMemSize(Fields& fields, Tab* tab);
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
};

/// \ingroup seed
/// 用來描述 Seed 的外觀:
/// - 透過 Fields 描述資料.
/// - 若有 DyMem, 則必需透過 MakeDyMemRaw() 建立資料.
class fon9_API Tab : public intrusive_ref_counter<Tab>, public NamedIx {
   fon9_NON_COPY_NON_MOVE(Tab);
public:
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
   Tab(const Named& named, Fields&& fields, LayoutSP saplingLayout = LayoutSP{});

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
