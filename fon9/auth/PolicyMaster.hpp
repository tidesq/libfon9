/// \file fon9/auth/PolicyMaster.hpp
///
/// AuthMgr
///  | [Agents]
///  +- Master(e.g. "PoAcl", "Role"...)
///      |  PolicyId | Fields (e.g. "Description", "Home"...)
///      +- trader01
///      |  |  [Detail]
///      |  |  key   | Fields (e.g. key="IvacNo", Fields="Name", "Rights", "CertNo"...)
///      |  +- 1234-10
///      |  +- 1234-21
///      |  \- 1234-32
///      |
///      |- guest
///      |
///      \- opridc
///
/// \author fonwinz@gmail.com
#ifndef __fon9_auth_PolicyMaster_hpp__
#define __fon9_auth_PolicyMaster_hpp__
#include "fon9/auth/PolicyAgent.hpp"
#include "fon9/MustLock.hpp"

namespace fon9 { namespace auth {

class fon9_API MasterPolicyTree;
using MasterPolicyTreeSP = intrusive_ptr<MasterPolicyTree>;

class fon9_API DetailPolicyTree;
using DetailPolicyTreeSP = intrusive_ptr<DetailPolicyTree>;

class fon9_API MasterPolicyItem : public PolicyItem {
   fon9_NON_COPY_NON_MOVE(MasterPolicyItem);
   using base = PolicyItem;
public:
   const MasterPolicyTreeSP  OwnerMasterTree_;

   MasterPolicyItem(const StrView& policyId, MasterPolicyTreeSP owner);

   /// 在衍生者建構時設定, 在 SetRemoved() 時 reset();
   DetailPolicyTreeSP   DetailPolicyTree_;
   seed::TreeSP GetSapling() override;
   void SetRemoved(PolicyTable&) override;

   // 由衍生者處理, 例如:
   // CharVector  Description_; // 其他 master policy 欄位.
   // void LoadPolicy(DcQueue& buf) override {
   //    unsigned ver = 0;
   //    DetailTable::Locker details{this->DetailPolicyTree_->DetailTable_};
   //    BitvInArchive{buf}(ver, this->Description_, *details);
   // }
   // void SavePolicy(RevBuffer& rbuf) override {
   //    const unsigned ver = 0;
   //    DetailTable::ConstLocker details{this->DetailPolicyTree_->DetailTable_};
   //    BitvOutArchive{rbuf}(ver, this->Description_, *details);
   // }
};
using MasterPolicyItemSP = intrusive_ptr<MasterPolicyItem>;

class fon9_API MasterPolicyTree : public PolicyTree {
   fon9_NON_COPY_NON_MOVE(MasterPolicyTree);
   using base = PolicyTree;
   friend class DetailPolicyTree; // for DetailPolicyTree::WriteUpdated();

public:
   using base::base;

   /// \code
   /// struct ResultHandler {
   ///    YourPolicyConfig* Result_;
   ///    void InLocking(const PolicyItem& item) {
   ///      在 ItemMap_ 鎖定的情況下設定 this->Result_;
   ///    }
   ///    void OnUnlocked(DetailPolicyTree& detailTree) {
   ///      在 ItemMap_ 已解鎖的情況下設定 this->Result_;
   ///      通常: lock detail table 然後複製到 this->Result_;
   ///    }
   /// };
   /// \endcode
   template <class ResultHandler>
   bool GetPolicy(const StrView& policyId, ResultHandler&& res) const {
      PolicyMaps::ConstLocker maps{this->PolicyMaps_};
      auto                    ifind = maps->ItemMap_.find(policyId);
      if (ifind == maps->ItemMap_.end())
         return false;
      res.InLocking(**ifind);
      DetailPolicyTreeSP detailTree = static_cast<MasterPolicyItem*>(ifind->get())->DetailPolicyTree_;
      maps.unlock();
      res.OnUnlocked(*detailTree);
      return true;
   }
};

//--------------------------------------------------------------------------//

class fon9_API DetailPolicyTree : public seed::Tree {
   fon9_NON_COPY_NON_MOVE(DetailPolicyTree);
   using base = seed::Tree;
public:
   const MasterPolicyItemSP   OwnerMasterItem_;

   using base::base;

   /// 因為所有 detail 的 layout 都相同,
   /// 所以 layout 使用: owner.OwnerMasterTree_->LayoutSP_->GetTab(0)->SaplingLayout_;
   DetailPolicyTree(MasterPolicyItem& owner)
      : base{owner.OwnerMasterTree_->LayoutSP_->GetTab(0)->SaplingLayout_}
      , OwnerMasterItem_(&owner) {
   }

   template <class LockedDetailTabble>
   void WriteUpdated(LockedDetailTabble& lockedTable) {
      lockedTable.unlock();
      PolicyMaps::Locker  maps{this->OwnerMasterItem_->OwnerMasterTree_->PolicyMaps_};
      maps->WriteUpdated(*this->OwnerMasterItem_);
   }
};

fon9_WARN_DISABLE_PADDING;
template <class DetailValue>
class DetailPolicyPodOp : public seed::PodOpDefault {
   fon9_NON_COPY_NON_MOVE(DetailPolicyPodOp);
   using base = PodOpDefault;
public:
   DetailValue&   Seed_;
   bool           IsModified_{false};
   DetailPolicyPodOp(DetailValue& seed, seed::Tree& sender, seed::OpResult res, const StrView& keyText)
      : base{sender, res, keyText}
      , Seed_(seed) {
   }
   void BeginRead(seed::Tab& tab, seed::FnReadOp fnCallback) override {
      this->BeginRW(tab, std::move(fnCallback), seed::SimpleRawRd{this->Seed_});
   }
   void BeginWrite(seed::Tab& tab, seed::FnWriteOp fnCallback) override {
      this->BeginRW(tab, std::move(fnCallback), seed::SimpleRawWr{this->Seed_});
      this->IsModified_ = true;
   }
};
fon9_WARN_POP;

struct KeyMakerCharVector {
   static CharVector StrToKey(const StrView& keyText) {
      return PolicyName::MakeRef(keyText);
   }
};

template <class DetailTableImpl, class KeyMaker>
class DetailPolicyTreeTable : public DetailPolicyTree {
   fon9_NON_COPY_NON_MOVE(DetailPolicyTreeTable);
   using base = DetailPolicyTree;
public:
   using PodKey = typename DetailTableImpl::key_type;
   using PodValue = typename DetailTableImpl::value_type;
   using DetailTable = MustLock<DetailTableImpl>;
   using DetailTableLocker = typename DetailTable::Locker;
   DetailTable DetailTable_;

   using base::base;

   fon9_MSC_WARN_DISABLE(4265 /* class has virtual functions, but destructor is not virtual. */);
   class TreeOp : public seed::TreeOp {
      fon9_NON_COPY_NON_MOVE(TreeOp);
      using base = seed::TreeOp;
   public:
      using PodOp = DetailPolicyPodOp<PodValue>;

      TreeOp(DetailPolicyTree& tree) : base(tree) {
      }

      virtual void GridView(const seed::GridViewRequest& req, seed::FnGridViewOp fnCallback) {
         seed::GridViewResult res{this->Tree_, req.Tab_};
         {
            DetailTableLocker   map{static_cast<DetailPolicyTreeTable*>(&this->Tree_)->DetailTable_};
            seed::MakeGridView(*map, this->GetIteratorForGv(*map, req.OrigKey_),
                               req, res, &seed::SimpleMakeRowView<typename DetailTableImpl::iterator>);
         } // unlock.
         fnCallback(res);
      }

      void OnPodOp(DetailTableLocker& lockedMap, const StrView& keyText, PodValue& value, seed::FnPodOp&& fnCallback, bool isForceWrite = false) {
         PodOp op{value, this->Tree_, seed::OpResult::no_error, keyText};
         fnCallback(op, &op);
         if (op.IsModified_ || isForceWrite)
            static_cast<DetailPolicyTree*>(&this->Tree_)->WriteUpdated(lockedMap);
      }
      virtual void Get(StrView strKeyText, seed::FnPodOp fnCallback) override {
         {
            DetailTableLocker lockedMap{static_cast<DetailPolicyTreeTable*>(&this->Tree_)->DetailTable_};
            auto              ifind = this->GetIteratorForPod(*lockedMap, strKeyText);
            if (ifind != lockedMap->end()) {
               this->OnPodOp(lockedMap, strKeyText, *ifind, std::move(fnCallback));
               return;
            }
         } // unlock.
         fnCallback(seed::PodOpResult{this->Tree_, seed::OpResult::not_found_key, strKeyText}, nullptr);
      }
      virtual void Add(StrView strKeyText, seed::FnPodOp fnCallback) override {
         if (this->IsTextBegin(strKeyText) || this->IsTextEnd(strKeyText)) {
            fnCallback(seed::PodOpResult{this->Tree_, seed::OpResult::not_found_key, strKeyText}, nullptr);
            return;
         }
         PodKey            key{KeyMaker::StrToKey(strKeyText)};
         DetailTableLocker lockedMap{static_cast<DetailPolicyTreeTable*>(&this->Tree_)->DetailTable_};
         auto              ifind = lockedMap->find(key);
         bool              isForceWrite = false;
         if (ifind == lockedMap->end()) {
            isForceWrite = true;
            using SeedValue = decltype(ifind->second);
            ifind = lockedMap->insert(PodValue{key, SeedValue{}}).first;
         }
         this->OnPodOp(lockedMap, strKeyText, *ifind, std::move(fnCallback), isForceWrite);
      }
      virtual void Remove(StrView strKeyText, seed::Tab* tab, seed::FnPodRemoved fnCallback) override {
         seed::PodRemoveResult res{this->Tree_, seed::OpResult::not_found_key, strKeyText, tab};
         PodKey                key{KeyMaker::StrToKey(strKeyText)};
         {
            DetailTableLocker  lockedMap{static_cast<DetailPolicyTreeTable*>(&this->Tree_)->DetailTable_};
            auto               ifind = lockedMap->find(key);
            if (ifind != lockedMap->end()) {
               lockedMap->erase(ifind);
               res.OpResult_ = seed::OpResult::removed_pod;
               static_cast<DetailPolicyTree*>(&this->Tree_)->WriteUpdated(lockedMap);
            }
         }
         fnCallback(res);
      }
   };
   fon9_MSC_WARN_POP;

   void OnTreeOp(seed::FnTreeOp fnCallback) {
      TreeOp op{*this};
      fnCallback(seed::TreeOpResult{this, seed::OpResult::no_error}, &op);
   }
};

} } // namespaces
#endif//__fon9_auth_PolicyMaster_hpp__
