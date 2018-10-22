/// \file fon9/seed/SeedSubr.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_SeedSubr_hpp__
#define __fon9_seed_SeedSubr_hpp__
#include "fon9/seed/Tab.hpp"
#include "fon9/seed/RawRd.hpp"
#include "fon9/Subr.hpp"

namespace fon9 { namespace seed {

fon9_WARN_DISABLE_PADDING;
class fon9_API SeedNotifyArgs {
   fon9_NON_COPY_NON_MOVE(SeedNotifyArgs);
protected:
   std::string    mutable CacheGV_;
   virtual void MakeGridView() const;
public:
   Tree&          Tree_;
   /// OpResult::removed_pod 時, 此處為 nullptr.
   Tab*           Tab_;
   /// - IsTextBegin(args.KeyText_): OnParentSeedClear() 的事件通知.
   /// - IsTextEnd(args.KeyText_):   整個資料表的異動, 例如: Apply 之後的通知.
   const StrView  KeyText_;
   /// 可能為 nullptr:
   /// - OpResult_ == OpResult::removed_pod || OpResult::removed_seed 
   const RawRd*   Rd_;

   enum class NotifyType {
      SeedChanged,
      PodRemoved,
      SeedRemoved,
      /// 整個資料表的異動, 例如 Apply 之後的通知.
      TableChanged,
      /// OnParentSeedClear() 的事件通知; 收到此通知之後, 訂閱會被清除, 不會再收到事件.
      ParentSeedClear,
   };
   NotifyType  NotifyType_;

   SeedNotifyArgs(Tree& tree, Tab* tab, const StrView& keyText, const RawRd* rd, NotifyType type)
      : Tree_(tree), Tab_(tab), KeyText_(keyText), Rd_(rd), NotifyType_(type) {
   }
   virtual ~SeedNotifyArgs();

   /// - NotifyType::SeedChanged:
   ///   不含 this->KeyText_, 僅包含 Tab_.Fields_, 頭尾不含分隔字元.
   /// - NotifyType::TableChanged:
   ///   通常是 TreeOp::GridView();
   const std::string& GetGridView() const {
      if (this->CacheGV_.empty())
         this->MakeGridView();
      return this->CacheGV_;
   }
};
fon9_WARN_POP;

using SeedSubr = std::function<void(const SeedNotifyArgs&)>;
using SeedSubj = Subject<SeedSubr>;

fon9_API void SeedSubj_NotifyPodRemoved(SeedSubj& subj, Tree& tree, StrView keyText);
fon9_API void SeedSubj_NotifySeedRemoved(SeedSubj& subj, Tree& tree, Tab& tab, StrView keyText);
fon9_API void SeedSubj_ParentSeedClear(SeedSubj& subj, Tree& tree);
fon9_API void SeedSubj_TableChanged(SeedSubj& subj, Tree& tree, Tab& tab);
fon9_API void SeedSubj_Notify(SeedSubj& subj, const SeedNotifyArgs& args);

template <class Rec>
inline void SeedSubj_Notify(SeedSubj& subj, Tree& tree, Tab& tab, StrView keyText, const Rec& rec) {
   if (!subj.IsEmpty()) {
      SimpleRawRd rd{rec};
      SeedSubj_Notify(subj, SeedNotifyArgs(tree, &tab, keyText, &rd, SeedNotifyArgs::NotifyType::SeedChanged));
   }
}

} } // namespaces
#endif//__fon9_seed_SeedSubr_hpp__
