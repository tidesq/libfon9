/// \file fon9/seed/Layout.cpp
/// \author fonwinz@gmail.com
#include "fon9/seed/Layout.hpp"
#include "fon9/seed/Tab.hpp"

namespace fon9 { namespace seed {

Layout::~Layout() {
}

//--------------------------------------------------------------------------//

TabSP Layout1::GetTab(StrView name) const {
   if (name == StrView{&this->KeyTab_->Name_})
      return this->KeyTab_;
   return TabSP{};
}
TabSP Layout1::GetTab(size_t index) const {
   return index == 0 ? this->KeyTab_ : TabSP{};
}
size_t Layout1::GetTabCount() const {
   return 1;
}

//--------------------------------------------------------------------------//

void LayoutN::InitTabIndex() {
   size_t index = 0;
   for (TabSP& tab : this->Tabs_)
      tab->SetIndex(index++);
   this->Tabs_.shrink_to_fit();
}
size_t LayoutN::GetTabCount() const {
   return this->Tabs_.size();
}
TabSP LayoutN::GetTab(StrView name) const {
   for (const TabSP& sp : this->Tabs_) {
      if (Tab* tab = sp.get())
         if (name == StrView{&tab->Name_})
            return tab->shared_from_this();
   }
   return TabSP{};
}
TabSP LayoutN::GetTab(size_t index) const {
   return(index < this->Tabs_.size() ? this->Tabs_[index] : TabSP{});
}

//--------------------------------------------------------------------------//

TabSP LayoutDy::GetTab(StrView name) const {
   ConstLocker lockedTabs(*this);
   return lockedTabs->Get(name);
}
TabSP LayoutDy::GetTab(size_t index) const {
   ConstLocker lockedTabs(*this);
   return lockedTabs->Get(index);
}
size_t LayoutDy::GetTabCount() const {
   ConstLocker lockedTabs(*this);
   return lockedTabs->size();
}

} } // namespaces
