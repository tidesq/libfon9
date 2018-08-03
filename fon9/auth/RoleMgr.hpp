/// \file fon9/auth/RoleMgr.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_auth_RoleMgr_hpp__
#define __fon9_auth_RoleMgr_hpp__
#include "fon9/auth/PolicyAgent.hpp"

namespace fon9 { namespace auth {

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

   bool GetRole(StrView roleId, RoleConfig& res);

   bool LinkStorage(const InnDbfSP& storage) {
      return base::LinkStorage(storage, 128);
   }
};

} } // namespaces
#endif//__fon9_auth_RoleMgr_hpp__
