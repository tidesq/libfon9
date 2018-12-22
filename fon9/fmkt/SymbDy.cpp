// \file fon9/fmkt/SymbDy.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbDy.hpp"
#include "fon9/fmkt/SymbRef.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/seed/Plugins.hpp"

namespace fon9 { namespace fmkt {

SymbDy::SymbDy(SymbDyTreeSP symbTreeSP, const StrView& symbid) : base{symbid}, SymbTreeSP_{std::move(symbTreeSP)} {
}
SymbData* SymbDy::GetSymbData(int tabid) {
   if (tabid <= 0)
      return this;
   ExDatas::Locker lk{this->ExDatas_};
   return (static_cast<size_t>(tabid) <= lk->size() ? (*lk)[static_cast<size_t>(tabid - 1)].get() : nullptr);
}
SymbData* SymbDy::FetchSymbData(int tabid) {
   if (tabid <= 0)
      return this;
   SymbDataTab* tab = dynamic_cast<SymbDataTab*>(this->SymbTreeSP_->LayoutSP_->GetTab(static_cast<size_t>(tabid)));
   if (tab == nullptr)
      return nullptr;
   ExDatas::Locker lk{this->ExDatas_};
   if (lk->size() < static_cast<size_t>(tabid))
      lk->resize(static_cast<size_t>(tabid));
   if (SymbData* dat = (*lk)[static_cast<size_t>(tabid - 1)].get())
      return dat;
   return ((*lk)[static_cast<size_t>(tabid - 1)] = tab->FetchSymbData(*this)).get();
}

//--------------------------------------------------------------------------//

SymbSP SymbDyTree::MakeSymb(const StrView& symbid) {
   return SymbSP{new SymbDy(this, symbid)};
}

static bool SymbMap_Start(seed::PluginsHolder& holder, StrView args) {
   // args = symbMapName.
   if (!StrTrim(&args).empty() && !ValidateName(args)) {
      holder.SetPluginsSt(LogLevel::Error, "Invalid SymbDy map name=", args);
      return false;
   }
   std::string    symbMapName = args.empty() ? "SymbDy" : args.ToString();
   seed::LayoutSP layout{new seed::LayoutDy(fon9_MakeField(Named{"Id"}, Symb, SymbId_),
      seed::TabSP{new seed::Tab{Named{"Base"}, Symb::MakeFields(), seed::TabFlag::NoSapling_NoSeedCommand_Writable}},
      seed::TreeFlag::AddableRemovable | seed::TreeFlag::Unordered)};
   SymbDyTree* dy = new SymbDyTree{std::move(layout)};
   if (holder.Root_->Add(new fon9::seed::NamedSapling(dy, std::move(symbMapName)))) {
      // 使用 SymbRef 驗證 SymbDy 機制.
      dy->AddSymbDataTab(new SymbRefTabDy(Named{"Ref"}));
   }
   return true;
}

} } // namespaces

extern "C" fon9_API fon9::seed::PluginsDesc f9p_SymbDyMap;
static fon9::seed::PluginsPark f9pRegister{"SymbDy", &f9p_SymbDyMap};

fon9::seed::PluginsDesc f9p_SymbDyMap{
   "",
   &fon9::fmkt::SymbMap_Start,
   nullptr,
   nullptr,
};
