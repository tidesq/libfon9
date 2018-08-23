/// \file fon9/seed/SeedFairy.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_SeedFairy_hpp__
#define __fon9_seed_SeedFairy_hpp__
#include "fon9/seed/MaTree.hpp"
#include "fon9/seed/SeedAcl.hpp"

namespace fon9 { namespace seed {

fon9_WARN_DISABLE_PADDING;
/// \ingroup seed
/// - 檢查存訪問的權限.
///   - 然後透過 SeedSearcher 協助您在 Tree、Pod、Seed 之間來回穿梭、查看、操作.
/// - 所有的操作都 **不是 thread safe**
struct fon9_API SeedFairy : public intrusive_ref_counter<SeedFairy> {
   fon9_NON_COPY_NON_MOVE(SeedFairy);
   struct AclTree;

   /// 透過 "/../" 來存取的資料表, e.g. "/../Acl" 可以查看 this->Ac_.Acl_ 的內容.
   const MaTreeSP SessionTree_;
   const MaTreeSP Root_;
   AclConfig      Ac_;
   AclPath        CurrPath_;

   SeedFairy(MaTreeSP root);
   ~SeedFairy();

   void Clear() {
      this->Ac_.Clear();
      this->CurrPath_.clear();
   }

   /// 視情況將 this->Ac_.Home_ 或 this->CurrPath_ 填入 outpath, 然後加上 seed.
   /// 然後正規化 outpath.
   /// \retval OpResult::path_format_error 格式有錯, seed.begin() 為 outpath 的錯誤位置.
   /// \retval OpResult::access_denied 無權限:
   ///      - 若 needsRights != AccessRight::None, 則必須要有 needsRights 的完整權限.
   ///      - 若 needsRights == AccessRight::None, 則有任一權限即可.
   OpResult NormalizeSeedPath(StrView& seed, AclPath& outpath, AccessRight needsRights = AccessRight::None) const;

   /// 傳回 this->Root_ 或 this->SessionTree_
   MaTree* GetRootPath(StrView& path) const;
};
using SeedFairySP = intrusive_ptr<SeedFairy>;
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_seed_SeedFairy_hpp__
