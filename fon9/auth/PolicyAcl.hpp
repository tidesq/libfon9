/// \file fon9/auth/PolicyAcl.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_auth_PolicyAcl_hpp__
#define __fon9_auth_PolicyAcl_hpp__
#include "fon9/auth/AuthMgr.hpp"
#include "fon9/seed/SeedAcl.hpp"

namespace fon9 { namespace auth {

/// \ingroup auth
/// Access control list 政策管理.
/// \code
/// AuthMgr/PoAcl.Layout =
///   { KeyName = "PolicyId",
///     Tab[0] = { TabName = "Acl", Fields[] = {"HomePath"},
///                Sapling.Layout =
///                  { KeyName = "Path",
///                    Tab[0] = { TabName = "AclRights", Fields[] = {"Rights"} }
///                  }
///              }
///   }
/// \endcode
/// | PolicyId | HomePath                |
/// |----------|-------------------------|
/// | admin    | /                       |
/// | user     | /home/{UserId} (detail) |
///
/// (detail):
/// | Path           | Rights                  |
/// |----------------|-------------------------|
/// | /home/{UserId} | xff                     |
/// | /..            | xff                     |
///  "/.." is SeedFairy.VisitorsTree_;
///  目前有提供: "/../Acl" 可查看此次登入的 Acl.
///
class fon9_API PolicyAclAgent : public PolicyAgent {
   fon9_NON_COPY_NON_MOVE(PolicyAclAgent);
   using base = PolicyAgent;

public:
   PolicyAclAgent(seed::MaTree* authMgrAgents, std::string name);

   #define fon9_kCSTR_PolicyAclAgent_Name    "PoAcl"
   /// 在 authMgr.Agents_ 上面新增一個 PolicyAclAgent.
   /// \retval nullptr   name 已存在.
   /// \retval !=nullptr 新增成功的 PolicyAclAgent 物件, 返回前會先執行 LinkStorage().
   static intrusive_ptr<PolicyAclAgent> Plant(AuthMgr& authMgr, std::string name = fon9_kCSTR_PolicyAclAgent_Name) {
      auto res = authMgr.Agents_->Plant<PolicyAclAgent>("PolicyAclAgent.Plant", std::move(name));
      if (res)
         res->LinkStorage(authMgr.Storage_, 128);
      return res;
   }

   using PolicyConfig = seed::AclConfig;
   bool GetPolicy(const AuthResult& authr, PolicyConfig& res);
};

} } // namespaces
#endif//__fon9_auth_PolicyAcl_hpp__
