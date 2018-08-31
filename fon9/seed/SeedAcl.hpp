/// \file fon9/seed/SeedAcl.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_SeedAcl_hpp__
#define __fon9_seed_SeedAcl_hpp__
#include "fon9/seed/Layout.hpp"
#include "fon9/seed/Tab.hpp"
#include "fon9/CharVector.hpp"
#include "fon9/SortedVector.hpp"

namespace fon9 { namespace seed {

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

/// \ingroup seed
/// Access control list.
struct AccessList : public SortedVector<AclPath, AccessRight> {
   using base = SortedVector<AclPath, AccessRight>;
   using base::lower_bound;
   iterator lower_bound(StrView strKeyText) { return this->lower_bound(key_type::MakeRef(strKeyText)); }
   const_iterator lower_bound(StrView strKeyText) const { return this->lower_bound(key_type::MakeRef(strKeyText)); }

   using base::find;
   iterator find(StrView strKeyText) { return this->find(key_type::MakeRef(strKeyText)); }
   const_iterator find(StrView strKeyText) const { return this->find(key_type::MakeRef(strKeyText)); }
};

/// \ingroup seed.
/// 用來存取 AccessList 的 layout.
/// - KeyField  = "Path";
/// - Tab[0]    = "AclRights";
/// - Fields[0] = "Rights";
fon9_API LayoutSP MakeAclTreeLayout();

/// \ingroup seed
struct AclConfig {
   /// 若有包含 "{UserId}" 則從 AclAgent 取出 PolicyAcl 時,
   /// 會用實際的 UserId 取代 "{UserId}",
   /// e.g. "/home/{UserId}" => "/home/fonwin"
   AclPath     Home_;
   AccessList  Acl_;

   void Clear() {
      this->Home_.clear();
      this->Acl_.clear();
   }

   /// - Home_ 設為 "/"
   /// - "/" 及 "/.." 權限設為 AccessRight::Full;
   void SetAdminMode() {
      this->Home_.assign("/");
      this->Acl_.kfetch(AclPath::MakeRef("/", 1)).second = AccessRight::Full;
      this->Acl_.kfetch(AclPath::MakeRef("/..", 3)).second = AccessRight::Full;
   }

   /// 沒任何權限?
   bool IsAccessDeny() const {
      return this->Acl_.empty();
   }
};

fon9_WARN_DISABLE_PADDING;
/// \ingroup seed
/// 正規化 seed path, 並透過正規劃結果尋找 Seed 存取權限設定.
/// 正規化之後: '/' 開頭, 尾端沒有 '/'
struct fon9_API AclPathParser {
   AclPath              Path_;
   std::vector<size_t>  IndexEndPos_;

   /// 正規化之後的路徑存放在 this->Path_;
   /// 若正規化失敗, 則 this 不會變動;
   /// \retval false  path 解析有誤, path.begin()==錯誤的位置.
   bool NormalizePath(StrView& path);

   /// 正規化之後的路徑存放在 this->Path_;
   /// 若正規化失敗, 則 this->Path_ = str; IndexEndPos_.empty()
   /// \retval !=nullptr 若 path 解析有誤, 則傳回錯誤的位置.
   /// \retval ==nullptr 解析正確完成.
   const char* NormalizePathStr(StrView path);

   /// 根據 Path_ 檢查 acl 的設定.
   /// - 必須先執行過 NormalizePath()
   /// - 優先使用完全一致的設定: 從後往前解析, 找到符合的權限設定.
   AccessRight GetAccess(const AccessList& acl) const;
   /// 若 needsRights != AccessRight::None,
   /// 則必須要有 needsRights 的完整權限, 才會傳回 acl 所設定的權限.
   AccessRight CheckAccess(const AccessList& acl, AccessRight needsRights) const;
};
fon9_WARN_POP;

/// \ingroup seed
/// 正規化 seedPath, 若 seedPath 符合規則, 則傳回正規化之後的字串.
/// 若 seedPath 不符合規則, 則傳回 seedPath.
fon9_API AclPath AclPathNormalize(StrView seedPath);

/// \ingroup seed
/// - 若 seedPath 為 "/.." 或 "/../" 開頭, 則為 VisitorsTree, 傳回移除 "/.." 之後的位置.
/// - 否則不是 VisitorsTree, 傳回 nullptr;
fon9_API const char* IsVisitorsTree(StrView seedPath);

} } // namespaces
#endif//__fon9_seed_SeedAcl_hpp__
