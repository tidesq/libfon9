/// \file fon9/seed/Layout.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_Layout_hpp__
#define __fon9_seed_Layout_hpp__
#include "fon9/seed/SeedBase.hpp"
#include "fon9/seed/NamedIx.hpp"
#include "fon9/MustLock.hpp"

namespace fon9 { namespace seed {

fon9_WARN_DISABLE_PADDING;
/// \ingroup seed
/// 透過 Layout 描述 Tree 所產的 Pod(Seed).
/// - 一個 Layout 包含 1~N 個 Tab
/// - 一個 Tab 描述一種 Seed.
/// - 每個 Layout 最少有一個 Tab: GetTab(0)==KeyTab.
class fon9_API Layout : public intrusive_ref_counter<Layout> {
   fon9_NON_COPY_NON_MOVE(Layout);
public:
   /// KeyField 不列入 Tab::Fields_.
   const FieldSPT<const Field>  KeyField_;

   Layout(FieldSP keyField) : KeyField_{std::move(keyField)} {
   }

   virtual ~Layout();
   virtual Tab* GetTab(StrView name) const = 0;
   virtual Tab* GetTab(size_t index) const = 0;
   virtual size_t GetTabCount() const = 0;
};
fon9_WARN_POP;

/// \ingroup seed
/// 每個 Pod 僅包含一顆 Seed.
class fon9_API Layout1 : public Layout {
   fon9_NON_COPY_NON_MOVE(Layout1);
public:
   const TabSP   KeyTab_;
   Layout1(FieldSP&& keyField, TabSP&& keyTab);
   ~Layout1();
   virtual Tab* GetTab(StrView name) const override;
   virtual Tab* GetTab(size_t index) const override;
   virtual size_t GetTabCount() const override;
};

/// \ingroup seed
/// 每個 Pod 包含固定數量的 Seeds.
class fon9_API LayoutN : public Layout {
   fon9_NON_COPY_NON_MOVE(LayoutN);
   std::vector<TabSP>  Tabs_;
   void InitTabIndex();
public:
   template <class... ArgsT>
   LayoutN(FieldSP&& keyField, ArgsT&&... args)
      : Layout{std::move(keyField)}
      , Tabs_{std::forward<ArgsT>(args)...} {
      this->InitTabIndex();
   }
   ~LayoutN();
   virtual size_t GetTabCount() const override;
   virtual Tab* GetTab(StrView name) const override;
   virtual Tab* GetTab(size_t index) const override;
};

/// \ingroup seed
/// 儲存 [Tab陣列] 的型別.
using TabsDy = NamedIxMapNoRemove<TabSP>;

/// \ingroup seed
/// 可動態增加 Tab 的 Layout.
/// 為了簡化設計, Tab 只允許增加, 不允許動態移除.
/// - 因為如果要提供 Tab 移除功能, 則已建立的 Pod 裡面對應的 Seed 也要跟著移除.
///   - 若已產出了大量 Pod, 會造成系統很大的負擔.
///   - 且 Tab 動態移除的需求不大, 如果真有需要, 則重啟程式使用新的 Layout 即可.
/// - 一般而言, [使用端]會在一開始, 就先將需要用到的 Tab 準備好, 可以降低資料取得的延遲.
class fon9_API LayoutDy : public Layout, public MustLock<TabsDy> {
   fon9_NON_COPY_NON_MOVE(LayoutDy);
public:
   LayoutDy(FieldSP&& keyField) : Layout{std::move(keyField)} {
   }
   LayoutDy(FieldSP&& keyField, TabSP&& keyTab) : Layout{std::move(keyField)} {
      Locker locker(*this);
      locker->Add(std::move(keyTab));
   }
   ~LayoutDy();
   virtual Tab* GetTab(StrView name) const override;
   virtual Tab* GetTab(size_t index) const override;
   virtual size_t GetTabCount() const override;
};

} } // namespaces
#endif//__fon9_seed_Layout_hpp__
