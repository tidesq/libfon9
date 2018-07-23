/// \file fon9/seed/MaTree.cpp
/// \author fonwinz@gmail.com
#include "fon9/seed/MaTree.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/Log.hpp"

namespace fon9 { namespace seed {

NamedSeed::~NamedSeed() {
}
void NamedSeed::OnParentTreeClear(Tree&) {
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

MaTree::MaTree(std::string coatName)
   : base{new Layout1(fon9_MakeField(Named{"Name"}, NamedSeed, Name_),
                      new Tab{Named{std::move(coatName)}, NamedSeed::MakeDefaultFields()})} {
}

void MaTree::OnMaTree_AfterAdd(NamedSeed& /*seed*/) {
}
void MaTree::OnMaTree_AfterRemove(NamedSeed& /*seed*/) {
}
void MaTree::OnMaTree_AfterClear() {
}

bool MaTree::Add(NamedSeedSP seed, StrView logErrHeader) {
   {
      Locker children{this->Children_};
      auto   ires = children->insert(ContainerImpl::value_type{&seed->Name_, std::move(seed)});
      if (ires.second) {
         this->OnMaTree_AfterAdd(*ires.first->second);
         return true;
      }
   } // unlock.
   if (!logErrHeader.empty())
      fon9_LOG_ERROR(logErrHeader, "|name=", seed->Name_, "|err=seed exists");
   return false;
}

std::vector<NamedSeedSP> MaTree::GetList(StrView nameHead) const {
   std::vector<NamedSeedSP>   res;
   ConstLocker                children{this->Children_};
   auto                       ifind{children->lower_bound(nameHead)};
   while (ifind != children->end()
          && ifind->first.size() >= nameHead.size()
          && memcmp(ifind->first.begin(), nameHead.begin(), nameHead.size()) == 0) {
      res.emplace_back(ifind->second);
      ++ifind;
   }
   return res;
}

NamedSeedSP MaTree::Remove(StrView name) {
   Locker   children{this->Children_};
   auto     ifind{children->find(name)};
   if (ifind == children->end())
      return nullptr;
   NamedSeedSP seed = ifind->second;
   children->erase(ifind);
   this->OnMaTree_AfterRemove(*seed);
   return seed;
}

void MaTree::ClearSeeds() {
   ContainerImpl temp;
   {
      Locker children{this->Children_};
      children->swap(temp);
      this->OnMaTree_AfterClear();
   }
   // unlock 之後 temp 死亡時自動清理 children, 清理前需要通知 child.
   for (auto& seed : temp)
      seed.second->OnParentTreeClear(*this);
}

//--------------------------------------------------------------------------//

fon9_WARN_DISABLE_PADDING;
fon9_MSC_WARN_DISABLE_NO_PUSH(4265 /* class has virtual functions, but destructor is not virtual. */);
struct MaTree::PodOp : public PodOpDefault {
   fon9_NON_COPY_NON_MOVE(PodOp);
   using base = PodOpDefault;

   NamedSeed&  Seed_;
   PodOp(NamedSeed& seed, Tree& sender, OpResult res, const StrView& key)
      : base{sender, res, key}
      , Seed_(seed) {
   }

   template <class RawRW, class FnOp>
   void BeginRW(Tab& tab, FnOp&& fnCallback) {
      assert(this->Sender_->LayoutSP_->GetTab(static_cast<size_t>(tab.GetIndex())) == &tab);
      this->Tab_ = &tab;
      RawRW op{this->Seed_};
      this->OpResult_ = OpResult::no_error;
      fnCallback(*this, &op);
   }
   void BeginRead(Tab& tab, FnReadOp fnCallback) override {
      this->BeginRW<SimpleRawRd>(tab, std::move(fnCallback));
   }
   void BeginWrite(Tab& tab, FnWriteOp fnCallback) override {
      this->BeginRW<SimpleRawWr>(tab, std::move(fnCallback));
   }
   virtual TreeSP GetSapling(Tab&) {
      return this->Seed_.GetSapling();
   }
   virtual void OnSeedCommand(Tab* tab, StrView cmd, FnCommandResultHandler resHandler) {
      this->Tab_ = tab;
      this->Seed_.OnSeedCommand(*this, cmd, std::move(resHandler));
   }
};
struct MaTree::TreeOp : public fon9::seed::TreeOp {
   fon9_NON_COPY_NON_MOVE(TreeOp);
   using base = fon9::seed::TreeOp;
   TreeOp(MaTree& tree) : base(tree) {
   }

   static ContainerImpl::iterator GetStartIterator(ContainerImpl& children, StrView strKeyText) {
      return base::GetStartIterator(children, strKeyText, [](const fon9::StrView& strKey) { return strKey; });
   }
   virtual void GridView(const GridViewRequest& req, FnGridViewOp fnCallback) {
      GridViewResult res{this->Tree_};
      {
         Locker children{static_cast<MaTree*>(&this->Tree_)->Children_};
         MakeGridView(*children, this->GetStartIterator(*children, req.OrigKey_),
                      req, res, &SimpleMakeRowView<ContainerImpl::iterator>);
      } // unlock.
      fnCallback(res);
   }

   virtual void Get(StrView strKeyText, FnPodOp fnCallback) override {
      NamedSeedSP seed;
      {
         Locker children{static_cast<MaTree*>(&this->Tree_)->Children_};
         auto   ifind = this->GetStartIterator(*children, strKeyText);
         if (ifind != children->end())
            seed = ifind->second;
      } // unlock.
      if (!seed)
         fnCallback(PodOpResult{this->Tree_, OpResult::not_found_key, strKeyText}, nullptr);
      else {
         PodOp op{*seed, this->Tree_, OpResult::no_error, strKeyText};
         fnCallback(op, &op);
      }
   }
   // MaTree 不支援透過 Op 來 Add(), Remove();
   // virtual void Add(StrView strKeyText, FnPodOp fnCallback) override;
   // virtual void Remove(StrView strKeyText, Tab* tab, FnPodRemoved fnCallback) override;
};
fon9_WARN_POP;

void MaTree::OnTreeOp(FnTreeOp fnCallback) {
   if (fnCallback) {
      TreeOp op{*this};
      fnCallback(TreeOpResult{this, OpResult::no_error}, &op);
   }
}

} } // namespaces
