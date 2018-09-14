/// \file fon9/seed/Layout.cpp
/// \author fonwinz@gmail.com
#include "fon9/seed/Layout.hpp"
#include "fon9/seed/Tab.hpp"

namespace fon9 { namespace seed {

Layout::~Layout() {
}

//--------------------------------------------------------------------------//

Layout1::Layout1(FieldSP&& keyField, TabSP&& keyTab, TreeFlag flags)
   : Layout(std::move(keyField), flags)
   , KeyTab_(keyTab) {
   this->KeyTab_->SetIndex(0);
}
Layout1::~Layout1() {
}
Tab* Layout1::GetTab(StrView name) const {
   if (name == StrView{&this->KeyTab_->Name_})
      return this->KeyTab_.get();
   return nullptr;
}
Tab* Layout1::GetTab(size_t index) const {
   return index == 0 ? this->KeyTab_.get() : nullptr;
}
size_t Layout1::GetTabCount() const {
   return 1;
}

//--------------------------------------------------------------------------//

LayoutN::~LayoutN() {
}
void LayoutN::InitTabIndex() {
   size_t index = 0;
   for (TabSP& tab : this->Tabs_)
      tab->SetIndex(index++);
   this->Tabs_.shrink_to_fit();
}
size_t LayoutN::GetTabCount() const {
   return this->Tabs_.size();
}
Tab* LayoutN::GetTab(StrView name) const {
   for (const TabSP& sp : this->Tabs_) {
      if (Tab* tab = sp.get())
         if (name == StrView{&tab->Name_})
            return tab;
   }
   return nullptr;
}
Tab* LayoutN::GetTab(size_t index) const {
   return(index < this->Tabs_.size() ? this->Tabs_[index].get() : nullptr);
}

//--------------------------------------------------------------------------//

LayoutDy::~LayoutDy() {
}
Tab* LayoutDy::GetTab(StrView name) const {
   ConstLocker lockedTabs(*this);
   return lockedTabs->Get(name);
}
Tab* LayoutDy::GetTab(size_t index) const {
   ConstLocker lockedTabs(*this);
   return lockedTabs->Get(index);
}
size_t LayoutDy::GetTabCount() const {
   ConstLocker lockedTabs(*this);
   return lockedTabs->size();
}

} } // namespaces
