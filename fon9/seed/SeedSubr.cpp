// \file fon9/seed/SeedSubr.cpp
// \author fonwinz@gmail.com
#include "fon9/seed/Tree.hpp"
#include "fon9/seed/ConfigGridView.hpp"
#include "fon9/RevPrint.hpp"
#include "fon9/CountDownLatch.hpp"

namespace fon9 { namespace seed {

SeedNotifyArgs::~SeedNotifyArgs() {
}
void SeedNotifyArgs::MakeGridView() const {
   if (this->NotifyType_ == NotifyType::TableChanged) {
      CountDownLatch waiter{1};
      this->Tree_.OnTreeOp([this, &waiter](const TreeOpResult&, TreeOp* op) {
         if (!op)
            waiter.ForceWakeUp();
         else
            op->GridView(GridViewRequestFull{*this->Tab_},
                         [this, &waiter](GridViewResult& res) {
               this->CacheGV_ = res.GridView_;
               waiter.ForceWakeUp();
            });
      });
      waiter.Wait();
      return;
   }
   if (this->Rd_ == nullptr || this->Tab_ == nullptr)
      return;
   if (size_t fldno = this->Tab_->Fields_.size()) {
      RevBufferList rbuf{128};
      for (;;) {
         this->Tab_->Fields_.Get(--fldno)->CellRevPrint(*this->Rd_, nullptr, rbuf);
         if (fldno == 0)
            break;
         RevPutChar(rbuf, *fon9_kCSTR_CELLSPL);
      }
      this->CacheGV_ = BufferTo<std::string>(rbuf.MoveOut());
   }
}

fon9_API void SeedSubj_Notify(SeedSubj& subj, const SeedNotifyArgs& args) {
   subj.Publish(args);
}
fon9_API void SeedSubj_NotifyPodRemoved(SeedSubj& subj, Tree& tree, StrView keyText) {
   if (!subj.IsEmpty())
      SeedSubj_Notify(subj, SeedNotifyArgs(tree, nullptr, keyText, nullptr, SeedNotifyArgs::NotifyType::PodRemoved));
}
fon9_API void SeedSubj_NotifySeedRemoved(SeedSubj& subj, Tree& tree, Tab& tab, StrView keyText) {
   if (!subj.IsEmpty())
      SeedSubj_Notify(subj, SeedNotifyArgs(tree, &tab, keyText, nullptr, SeedNotifyArgs::NotifyType::SeedRemoved));
}
fon9_API void SeedSubj_ParentSeedClear(SeedSubj& subj, Tree& tree) {
   if (!subj.IsEmpty()) {
      SeedSubj_Notify(subj, SeedNotifyArgs(tree, nullptr, TextBegin(), nullptr, SeedNotifyArgs::NotifyType::ParentSeedClear));
      subj.Clear();
   }
}
fon9_API void SeedSubj_TableChanged(SeedSubj& subj, Tree& tree, Tab& tab) {
   if (!subj.IsEmpty())
      SeedSubj_Notify(subj, SeedNotifyArgs(tree, &tab, TextEnd(), nullptr, SeedNotifyArgs::NotifyType::TableChanged));
}


} } // namespaces
