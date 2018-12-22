// \file fon9/fmkt/SymbIn.cpp
// \author fonwinz@gmail.com
#include "fon9/fmkt/SymbIn.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/seed/Plugins.hpp"

namespace fon9 { namespace fmkt {

static const int32_t kSymbInOffset[]{
   0,
   fon9_OffsetOf(SymbIn, Ref_),
};
static inline SymbData* GetSymbInData(SymbIn* pthis, int tabid) {
   return static_cast<size_t>(tabid) < numofele(kSymbInOffset)
      ? reinterpret_cast<SymbData*>(reinterpret_cast<char*>(pthis) + kSymbInOffset[tabid])
      : nullptr;
}

SymbData* SymbIn::GetSymbData(int tabid) {
   return GetSymbInData(this, tabid);
}
SymbData* SymbIn::FetchSymbData(int tabid) {
   return GetSymbInData(this, tabid);
}

//--------------------------------------------------------------------------//

SymbSP SymbInTree::MakeSymb(const StrView& symbid) {
   return SymbSP{new SymbIn(symbid)};
}

static bool SymbMap_Start(seed::PluginsHolder& holder, StrView args) {
   // args = symbMapName.
   if (!StrTrim(&args).empty() && !ValidateName(args)) {
      holder.SetPluginsSt(LogLevel::Error, "Invalid SymbIn map name=", args);
      return false;
   }
   std::string    symbMapName = args.empty() ? "SymbIn" : args.ToString();
   seed::LayoutSP layout{new seed::LayoutN(fon9_MakeField(Named{"Id"}, Symb, SymbId_),
      seed::TreeFlag::AddableRemovable | seed::TreeFlag::Unordered,
      seed::TabSP{new seed::Tab{Named{"Base"}, Symb::MakeFields(),    seed::TabFlag::NoSapling_NoSeedCommand_Writable}},
      seed::TabSP{new seed::Tab{Named{"Ref"},  SymbRef::MakeFields(), seed::TabFlag::NoSapling_NoSeedCommand_Writable}}
      )};
   holder.Root_->Add(new fon9::seed::NamedSapling(new SymbInTree{std::move(layout)}, std::move(symbMapName)));
   return true;
}
} } // namespaces

extern "C" fon9_API fon9::seed::PluginsDesc f9p_SymbInMap;
static fon9::seed::PluginsPark f9pRegister{"SymbIn", &f9p_SymbInMap};

fon9::seed::PluginsDesc f9p_SymbInMap{
   "",
   &fon9::fmkt::SymbMap_Start,
   nullptr,
   nullptr,
};
