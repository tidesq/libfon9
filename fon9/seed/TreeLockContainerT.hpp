/// \file fon9/seed/TreeLockContainerT.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_TreeLockContainerT_hpp__
#define __fon9_seed_TreeLockContainerT_hpp__
#include "fon9/seed/Tree.hpp"
#include "fon9/seed/PodOp.hpp"
#include "fon9/seed/RawWr.hpp"
#include "fon9/MustLock.hpp"

namespace fon9 { namespace seed {

fon9_MSC_WARN_DISABLE(4265 /* class has virtual functions, but destructor is not virtual. */);
template <class Pod, class Locker>
class PodOpLocker : public PodOpDefault {
   fon9_NON_COPY_NON_MOVE(PodOpLocker);
   using base = PodOpDefault;

public:
   Pod&     Pod_;
   Locker&  Locker_;
   PodOpLocker(Pod& pod, Tree& sender, OpResult res, const StrView& key, Locker& locker)
      : base{sender, res, key}
      , Pod_(pod)
      , Locker_(locker) {
   }

   void Lock() {
      if (!this->Locker_.owns_lock())
         this->Locker_.lock();
   }
   void Unlock() {
      if (this->Locker_.owns_lock())
         this->Locker_.unlock();
   }

   void BeginRead(Tab& tab, FnReadOp fnCallback) override {
      this->Lock();
      this->BeginRW(tab, std::move(fnCallback), SimpleRawRd{this->Pod_.GetSeedRW(tab)});
   }
   void BeginWrite(Tab& tab, FnWriteOp fnCallback) override {
      this->Lock();
      this->BeginRW(tab, std::move(fnCallback), SimpleRawWr{this->Pod_.GetSeedRW(tab)});
   }

   /// TreeSP 是一個操作單元, 所以在取出 sapling 之後會 unlock.
   /// 避免在操作 sapling 時回頭需要 lock, 造成死結.
   TreeSP GetSapling(Tab& tab) override {
      TreeSP res = this->Pod_.HandleGetSapling(tab);
      this->Unlock();
      return res;
   }
   void OnSeedCommand(Tab* tab, StrView cmd, FnCommandResultHandler resHandler) override {
      this->Unlock();
      this->Tab_ = tab;
      this->Pod_.HandleSeedCommand(*this, cmd, std::move(resHandler));
   }
};
fon9_MSC_WARN_POP;

//--------------------------------------------------------------------------//

template <class Tree, class Container>
inline auto OnTreeClearSeeds(Tree& owner, Container& c)
-> decltype(c.begin()->OnParentTreeClear(owner), void()) {
   // container.value_type is value;
   for (auto& seed : c)
      seed.OnParentTreeClear(owner);
}
template <class Tree, class Container>
inline auto OnTreeClearSeeds(Tree& owner, Container& c)
-> decltype((*c.begin())->OnParentTreeClear(owner), void()) {
   // container.value_type is value*;
   for (auto& seed : c)
      seed->OnParentTreeClear(owner);
}
template <class Tree, class Container>
inline auto OnTreeClearSeeds(Tree& owner, Container& c)
-> decltype(c.begin()->second.OnParentTreeClear(owner), void()) {
   // container.value_type is std::pair<key,value>;
   for (auto& seed : c)
      seed.second.OnParentTreeClear(owner);
}
template <class Tree, class Container>
inline auto OnTreeClearSeeds(Tree& owner, Container& c)
-> decltype(c.begin()->second->OnParentTreeClear(owner), void()) {
   // container.value_type is std::pair<key,value*>;
   for (auto& seed : c)
      seed.second->OnParentTreeClear(owner);
}

/// \ingroup seed
/// - 在 Tree 裡面包含一個 MustLock<ContainerImplT>
template <class ContainerImplT>
class TreeLockContainerT : public Tree {
   fon9_NON_COPY_NON_MOVE(TreeLockContainerT);
   using base = Tree;

protected:
   using ContainerImpl = ContainerImplT;
   using Container = MustLock<ContainerImpl>;
   using Locker = typename Container::Locker;
   using ConstLocker = typename Container::ConstLocker;
   Container   Container_;

public:
   template <class... ArgsT>
   TreeLockContainerT(LayoutSP layout, ArgsT&&... args)
      : base{std::move(layout)}
      , Container_{std::forward<ArgsT>(args)...} {
   }

   size_t size() const {
      ConstLocker container{this->Container_};
      return container->size();
   }

   /// 清除全部的元素(Seeds).
   /// - 避免循環相依造成的 resource leak
   /// - 清理時透過 NamedSeed::OnParentTreeClear() 通知 seed.
   virtual void OnParentSeedClear() override {
      ContainerImpl temp;
      {
         Locker container{this->Container_};
         container->swap(temp);
      }
      OnTreeClearSeeds(*this, temp);
   }
};

} } // namespaces
#endif//__fon9_seed_TreeLockContainerT_hpp__
