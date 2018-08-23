// \file fon9/seed/SeedFairy.cpp
// \author fonwinz@gmail.com
#include "fon9/seed/SeedFairy.hpp"
#include "fon9/seed/FieldMaker.hpp"

namespace fon9 { namespace seed {

struct SeedFairy::AclTree : public Tree {
   fon9_NON_COPY_NON_MOVE(AclTree);
   AccessList& Acl_;
   AclTree(AccessList& acl)
      : Tree(MakeAclTreeLayout(), TreeFlag::Shadow)
      , Acl_(acl) {
   }

   fon9_MSC_WARN_DISABLE(4265 /* class has virtual functions, but destructor is not virtual. */);
   struct TreeOp : public seed::TreeOp {
      fon9_NON_COPY_NON_MOVE(TreeOp);
      using base = seed::TreeOp;
      TreeOp(AclTree& tree) : base(tree) {
      }
      void GridView(const GridViewRequest& req, FnGridViewOp fnCallback) override {
         GridViewResult res{this->Tree_, req.Tab_};
         AccessList&    acl = static_cast<AclTree*>(&this->Tree_)->Acl_;
         MakeGridView(acl, this->GetIteratorForGv(acl, req.OrigKey_),
                      req, res, &SimpleMakeRowView<AccessList::iterator>);
         fnCallback(res);
      }
      void Get(StrView strKeyText, FnPodOp fnCallback) override {
         AccessList& acl = static_cast<AclTree*>(&this->Tree_)->Acl_;
         auto        ifind = this->GetIteratorForPod(acl, strKeyText);
         if (ifind == acl.end())
            fnCallback(PodOpResult{this->Tree_, OpResult::not_found_key, strKeyText}, nullptr);
         else {
            using PodOpAcl = PodOpReadonly<AccessList::value_type>;
            PodOpAcl op{*ifind, this->Tree_, strKeyText};
            fnCallback(op, &op);
         }
      }
   };
   fon9_MSC_WARN_POP;
   void OnTreeOp(FnTreeOp fnCallback) override {
      TreeOp op{*this};
      fnCallback(TreeOpResult{this, OpResult::no_error}, &op);
   }
};

//--------------------------------------------------------------------------//

SeedFairy::SeedFairy(MaTreeSP root)
   : SessionTree_{new MaTree{"STree"}}
   , Root_{std::move(root)} {
   this->SessionTree_->Add(new NamedSapling(new AclTree{this->Ac_.Acl_}, "Acl"));
}
SeedFairy::~SeedFairy() {
   this->SessionTree_->OnParentSeedClear();
}

OpResult SeedFairy::NormalizeSeedPath(StrView& seed, AclPath& outpath, AccessRight needsRights) const {
   switch (seed.Get1st()) {
   case '/':
      break;
   case '~':
      seed.SetBegin(seed.begin() + 1);
      outpath = this->Ac_.Home_;
      outpath.push_back('/');
      break;
   default:
   case '.':
      outpath = this->CurrPath_;
      outpath.push_back('/');
      break;
   }
   seed.AppendTo(outpath);
   seed = ToStrView(outpath);

   AclPathParser pathParser;
   if (!pathParser.NormalizePath(seed))
      return OpResult::path_format_error;
   AccessRight rights = pathParser.CheckAccess(this->Ac_.Acl_, needsRights);
   outpath = std::move(pathParser.Path_);
   return rights == AccessRight::None ? OpResult::access_denied : OpResult::no_error;
}

MaTree* SeedFairy::GetRootPath(StrView& path) const {
   if (const char* pbeg = IsSessionTree(path)) {
      path.SetBegin(pbeg);
      return this->SessionTree_.get();
   }
   return this->Root_.get();
}


} } // namespaces
