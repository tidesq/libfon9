/// \file fon9/auth/RoleMgr.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_auth_RoleMgr_hpp__
#define __fon9_auth_RoleMgr_hpp__
#include "fon9/auth/PolicyAgent.hpp"

namespace fon9 { namespace auth {

/// \ingroup auth
/// RoleMgr::GetRole(roleId) 的結果 callback.
/// 不論結果是否正確, res.RoleId_ 都必定是當初要求的 roleId.
using FnOnGetRoleCB = std::function<void(AuthR authr, RoleConfig res)>;

class RoleTree : public PolicyTree {
   fon9_NON_COPY_NON_MOVE(RoleTree);
   using base = PolicyTree;
   using RoleTreeSP = seed::TreeSPT<RoleTree>;
   using PolicyConfigs = MustLock<PolicyKeys>;
   struct RoleItem;
   using RoleItemSP = intrusive_ptr<RoleItem>;

   /// 此角色的各個政策設定.
   /// 若某個 PolicyName 在此表找不到,
   /// 則應直接用 RoleId 當作該 PolicyName 的 PolicyId.
   struct RoleConfigTree : public seed::Tree {
      fon9_NON_COPY_NON_MOVE(RoleConfigTree);
      using base = seed::Tree;
      using SeedKey = PolicyName;
      using SeedValue = PolicyId;
      struct PodOp;
      struct TreeOp;
      PolicyConfigs     PolicyConfigs_;
      const RoleItemSP  OwnerRole_;

      RoleConfigTree(RoleItem& owner);
      void WriteUpdated(PolicyConfigs::Locker& lockedMap);
      void OnTreeOp(seed::FnTreeOp fnCallback) override;
   };
   using RoleCfgSP = seed::TreeSPT<RoleConfigTree>;

   struct RoleItem : public PolicyItem {
      fon9_NON_COPY_NON_MOVE(RoleItem);
      const RoleTreeSP  OwnerTree_;
      CharVector        Description_;
      RoleCfgSP         RoleConfig_; // 在 SetRemoved() 時 reset();
      RoleItem(const StrView& roleId, RoleTreeSP owner);
      seed::TreeSP GetSapling() override;
      void LoadPolicy(DcQueue&) override;
      void SavePolicy(RevBuffer&) override;
      void SetRemoved(PolicyTable& owner) override;
   };

   virtual PolicyItemSP MakePolicy(const StrView& roleId) override;

   static seed::LayoutSP MakeLayout();

public:
   RoleTree() : base{MakeLayout()} {
   }

   AuthR GetRole(StrView roleId, RoleConfig& res) const;
};

/// \ingroup auth
/// 使用者 **角色** 管理員.
/// - 透過 RoleId 取得各個 Policy 的設定.
/// - RoleMgr 是一種特殊的 Policy Agent, 由 AuthMgr 在建構時主動加入.
/// - 每個 RoleItem 包含一個 PolicyKeys;
class fon9_API RoleMgr : public PolicyAgent {
   fon9_NON_COPY_NON_MOVE(RoleMgr);
   using base = PolicyAgent;

public:
   RoleMgr(std::string name);

   void GetRole(StrView roleId, FnOnGetRoleCB cb);

   bool LinkStorage(InnDbf& storage) {
      return base::LinkStorage(storage, 128);
   }
};

} } // namespaces
#endif//__fon9_auth_RoleMgr_hpp__
