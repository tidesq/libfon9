/// \file fon9/auth/PolicyAcl.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_auth_PolicyAcl_hpp__
#define __fon9_auth_PolicyAcl_hpp__
#include "fon9/auth/AuthMgr.hpp"

namespace fon9 { namespace auth {

/// \ingroup seed
/// Seed 的存取權限旗標.
enum class AccessRight : uint8_t {
   None = 0x00,
   Full = 0xff,
   /// 允許讀取欄位內容 or GridView.
   Read = 0x01,
   /// 允許修改欄位內容.
   Write = 0x02,
   /// 允許執行 OnSeedCommand().
   Exec = 0x04,
};
fon9_ENABLE_ENUM_BITWISE_OP(AccessRight);

using AclPath = CharVector;

/// \ingroup auth
/// Access control list.
struct AccessList : public std::map<AclPath, AccessRight> {
   using base = std::map<AclPath, AccessRight>;
   using base::lower_bound;
   iterator lower_bound(StrView strKeyText) { return this->lower_bound(key_type::MakeRef(strKeyText)); }
   const_iterator lower_bound(StrView strKeyText) const { return this->lower_bound(key_type::MakeRef(strKeyText)); }

   using base::find;
   iterator find(StrView strKeyText) { return this->find(key_type::MakeRef(strKeyText)); }
   const_iterator find(StrView strKeyText) const { return this->find(key_type::MakeRef(strKeyText)); }
};

/// \ingroup auth
struct AclConfig {
   /// 若有包含 "{UserId}" 則從 AclAgent 取出 PolicyAcl 時,
   /// 會用實際的 UserId 取代 "{UserId}",
   /// e.g. "/home/{UserId}" => "/home/fonwin"
   AclPath     Home_;
   AccessList  Acl_;
};

/// \ingroup auth
/// 正規化 seed path, 並透過正規劃結果尋找 Seed 存取權限設定.
struct AclPathParser {
   AclPath              Path_;
   std::vector<size_t>  EndNames_;

   /// 正規化之後的路徑存放在 this->Path_;
   /// 若正規化失敗, 則 this 不會變動;
   /// \retval false  path 解析有誤, path.begin()==錯誤的位置.
   bool NormalizePath(StrView& path);

   /// 正規化之後的路徑存放在 this->Path_;
   /// 若正規化失敗, 則 this->Path_ = str; EndNames_.empty()
   /// \retval !=nullptr 若 path 解析有誤, 則傳回錯誤的位置.
   /// \retval ==nullptr 解析正確完成.
   const char* NormalizePathStr(StrView path);

   /// 根據 Path_ 檢查 acl 的設定.
   /// - 必須先執行過 NormalizePath()
   /// - 優先使用完全一致的設定: 從後往前解析, 找到符合的權限設定.
   AccessRight GetAccess(const AccessList& acl) const;
   /// 若 needsAccess != AccessRight::None,
   /// 則必須要有 needsAccess 的完整權限, 才會傳回 acl 所設定的權限.
   AccessRight CheckAccess(const AccessList& acl, AccessRight needsAccess) const;

   /// 在正規化之後的 Path_ 加上前置路徑.
   /// - this->Path_ = prefix + "/" + this->Path_;
   /// - 並配合調整 EndNames_
   void AddPrefix(StrView prefix);
   void DelPrefix();
   /// 取得正規化之後的路徑裡面, 第 index 個的路徑.
   StrView GetIndexPath(size_t index) const;
};

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
///
class fon9_API PolicyAclAgent : public PolicyAgent {
   fon9_NON_COPY_NON_MOVE(PolicyAclAgent);
   using base = PolicyAgent;

public:
   PolicyAclAgent(seed::MaTree* authMgrAgents, std::string name);

   /// 在 authMgr.Agents_ 上面新增一個 PolicyAclAgent.
   /// \retval nullptr   name 已存在.
   /// \retval !=nullptr 新增成功的 PolicyAclAgent 物件, 返回前會先執行 LinkStorage().
   static intrusive_ptr<PolicyAclAgent> Plant(AuthMgr& authMgr, std::string name) {
      auto res = authMgr.Agents_->Plant<PolicyAclAgent>("PolicyAclAgent.Plant", std::move(name));
      if (res)
         res->LinkStorage(*authMgr.Storage_, 128);
      return res;
   }

   bool GetPolicy(const AuthResult& authr, AclConfig& res);
};

} } // namespaces
#endif//__fon9_auth_PolicyAcl_hpp__
