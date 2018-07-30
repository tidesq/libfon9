/// \file fon9/seed/MaTree.cpp
/// \author fonwinz@gmail.com
#include "fon9/seed/MaTree.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/Log.hpp"

namespace fon9 { namespace seed {

NamedSeed::~NamedSeed() {
}
void NamedSeed::OnParentTreeClear(Tree&) {
   if (TreeSP sapling = this->GetSapling())
      sapling->OnParentSeedClear();
}
void NamedSeed::OnNamedSeedReleased() {
   if (this->use_count() == 0)
      delete this;
}
Fields NamedSeed::MakeDefaultFields() {
   Fields fields;
   fields.Add(fon9_MakeField(Named{"Title"},       NamedSeed, Title_));
   fields.Add(fon9_MakeField(Named{"Description"}, NamedSeed, Description_));
   return fields;
}

//--------------------------------------------------------------------------//

void NamedSeed::OnSeedCommand(SeedOpResult& res, StrView cmd, FnCommandResultHandler resHandler) {
   (void)cmd;
   res.OpResult_ = OpResult::not_supported_cmd;
   resHandler(res, nullptr);
}
TreeSP NamedSeed::GetSapling() {
   return nullptr;
}

TreeSP NamedSapling::GetSapling() {
   return this->Sapling_;
}

//--------------------------------------------------------------------------//

MaTree::MaTree(std::string tabName)
   : base{new Layout1(fon9_MakeField(Named{"Name"}, NamedSeed, Name_),
                      new Tab{Named{std::move(tabName)}, NamedSeed::MakeDefaultFields()})} {
}

void MaTree::OnMaTree_AfterAdd(Locker&, NamedSeed&) {
}
void MaTree::OnMaTree_AfterRemove(Locker&, NamedSeed&) {
}
void MaTree::OnMaTree_AfterClear() {
}

bool MaTree::Add(NamedSeedSP seed, StrView logErrHeader) {
   {
      Locker container{this->Container_};
      auto   ires = container->insert(std::move(seed));
      if (ires.second) {
         this->OnMaTree_AfterAdd(container, **ires.first);
         return true;
      }
   } // unlock.
   if (!logErrHeader.empty())
      fon9_LOG_ERROR(logErrHeader, "|name=", seed->Name_, "|err=seed exists");
   return false;
}

std::vector<NamedSeedSP> MaTree::GetList(StrView nameHead) const {
   std::vector<NamedSeedSP>   res;
   ConstLocker                container{this->Container_};
   auto                       ifind{container->lower_bound(nameHead)};
   while (ifind != container->end()) {
      NamedSeed& seed = **ifind;
      if (seed.Name_.size() < nameHead.size()
      || memcmp(seed.Name_.c_str(), nameHead.begin(), nameHead.size()) != 0)
         break;
      res.emplace_back(&seed);
      ++ifind;
   }
   return res;
}

NamedSeedSP MaTree::Remove(StrView name) {
   Locker   container{this->Container_};
   auto     ifind{container->find(name)};
   if (ifind == container->end())
      return nullptr;
   NamedSeedSP seed = *ifind;
   container->erase(ifind);
   this->OnMaTree_AfterRemove(container, *seed);
   return seed;
}

void MaTree::OnParentSeedClear() {
   base::OnParentSeedClear();
   this->OnMaTree_AfterClear();
}

//--------------------------------------------------------------------------//

fon9_WARN_DISABLE_PADDING;
fon9_MSC_WARN_DISABLE_NO_PUSH(4265 /* class has virtual functions, but destructor is not virtual. */
                              4355 /* 'this' : used in base member initializer list*/);
struct MaTree::PodOp : public PodOpLocker<PodOp, Locker> {
   fon9_NON_COPY_NON_MOVE(PodOp);
   using base = PodOpLocker<PodOp, Locker>;
   NamedSeed*  Seed_;
   PodOp(ContainerImpl::value_type& v, Tree& sender, OpResult res, const StrView& key, Locker& locker)
      : base{*this, sender, res, key, locker}
      , Seed_{v.get()} {
   }
   NamedSeed& GetSeedRW(Tab&) {
      return *this->Seed_;
   }
   TreeSP HandleGetSapling(Tab&) {
      return this->Seed_->GetSapling();
   }
   void HandleSeedCommand(SeedOpResult& res, StrView cmd, FnCommandResultHandler&& resHandler) {
      this->Seed_->OnSeedCommand(res, cmd, std::move(resHandler));
   }
};

struct MaTree::TreeOp : public fon9::seed::TreeOp {
   fon9_NON_COPY_NON_MOVE(TreeOp);
   using base = fon9::seed::TreeOp;
   using base::base;
   TreeOp(MaTree& tree) : base(tree) {
   }

   static void MakeNamedSeedView(NamedSeedContainerImpl::iterator ivalue, Tab* tab, RevBuffer& rbuf) {
      if (tab)
         FieldsCellRevPrint(tab->Fields_, SimpleRawRd{**ivalue}, rbuf, GridViewResult::kCellSplitter);
      RevPrint(rbuf, (**ivalue).Name_);
   }
   void GridView(const GridViewRequest& req, FnGridViewOp fnCallback) override {
      TreeOp_GridView_MustLock(*this, static_cast<MaTree*>(&this->Tree_)->Container_,
                               req, std::move(fnCallback), &MakeNamedSeedView);
   }

   void Get(StrView strKeyText, FnPodOp fnCallback) override {
      TreeOp_Get_MustLock<PodOp>(*this, static_cast<MaTree*>(&this->Tree_)->Container_, strKeyText, std::move(fnCallback));
   }

   // MaTree 不支援透過 Op 來 Add(), Remove();
   // void Add(StrView strKeyText, FnPodOp fnCallback) override;
   // void Remove(StrView strKeyText, Tab* tab, FnPodRemoved fnCallback) override;
};
fon9_WARN_POP;

void MaTree::OnTreeOp(FnTreeOp fnCallback) {
   if (fnCallback) {
      TreeOp op{*this};
      fnCallback(TreeOpResult{this, OpResult::no_error}, &op);
   }
}

} } // namespaces
