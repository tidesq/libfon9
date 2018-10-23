// \file fon9/seed/SeedSearcher.cpp
// \author fonwinz@gmail.com
#include "fon9/seed/SeedSearcher.hpp"
#include "fon9/seed/Tree.hpp"
#include "fon9/FilePath.hpp"

namespace fon9 { namespace seed {

//--------------------------------------------------------------------------//

fon9_API StrView ParseKeyTextAndTabName(StrView& tabName) {
   StrView keyText = SbrFetchInsideNoTrim(tabName, StrBrArg::Quotation_);
   if (keyText.begin() == nullptr) { // 沒有引號時 keyText 為 nullptr, 此時採用無引號的方法.
      keyText = StrFetchNoTrim(tabName, '^');
      return keyText.empty() ? TextBegin() : keyText;
   }
   // 執行到此: keyText = 移除引號後的字串, tabName = 引號之後的字串.
   // 所以 tab = 移除 '^'(包含) 之前的字元.
   StrFetchNoTrim(tabName, '^');
   return keyText;
}

//--------------------------------------------------------------------------//

SeedSearcher::~SeedSearcher() {
}
void SeedSearcher::OnLastStep(TreeOp& opTree, StrView keyText, Tab& tab) {
   this->ContinueSeed(opTree, keyText, tab);
}
void SeedSearcher::ContinueSeed(TreeOp& opTree, StrView keyText, Tab& tab) {
   opTree.Get(keyText, PodHandler{this, keyText.begin(), &tab});
}
TreeSP SeedSearcher::ContinueTree(PodOp& opPod, Tab& tab) {
   return opPod.GetSapling(tab);
}
void SeedSearcher::ContinuePod(TreeOp& opTree, StrView keyText, StrView tabName) {
   Tab* tab = tabName.empty() ? opTree.Tree_.LayoutSP_->GetTab(0) : opTree.Tree_.LayoutSP_->GetTab(tabName);
   if (tab) {
      if (this->RemainPath_.empty())
         this->OnLastStep(opTree, keyText, *tab);
      else
         this->ContinueSeed(opTree, keyText, *tab);
   }
   else {
      this->RemainPath_.SetBegin(tabName.empty() && keyText.begin() != nullptr ? keyText.begin() : tabName.begin());
      this->OnError(OpResult::not_found_tab);
   }
}

//--------------------------------------------------------------------------//

static void ContinueSeedSearch(Tree& curr, SeedSearcherSP searcher) {
   struct TreeOpHandler {
      SeedSearcherSP Searcher_;
      StrView        TabName_;
      StrView        KeyText_;
      TreeOpHandler(SeedSearcher* searcher)
         : Searcher_{searcher}
         , TabName_{nullptr} {
         if (!searcher->RemainPath_.empty()) {
            this->TabName_ = SbrFetchNoTrim(searcher->RemainPath_, '/', StrBrArg::Quotation_);
            searcher->RemainPath_ = FilePath::RemovePathHead(searcher->RemainPath_);
         }
      }
      void operator()(const TreeOpResult& resTree, TreeOp* opTree) {
         if (opTree == nullptr)
            this->Searcher_->OnError(resTree.OpResult_);
         else if (this->TabName_.begin() == nullptr) // 建構時 searcher->RemainPath_.empty()
            this->Searcher_->OnFoundTree(*opTree);
         else
            this->Searcher_->ContinuePod(*opTree, this->KeyText_, this->TabName_);
      }
   };

   // 如果現在是 Thread-A.
   // 則 hdr.operator(); 可能仍在 Thread-A, 但也可能在 Thread-B.
   TreeOpHandler hdr(searcher.get());
   if (fon9_UNLIKELY(hdr.TabName_.Get1st() == '^')) {
      if (const char* pspl = hdr.TabName_.Find(':')) {
         hdr.KeyText_.Reset(hdr.TabName_.begin() + 1, pspl);
         hdr.TabName_.SetBegin(pspl + 1);
         curr.OnTabTreeOp(hdr);
         return;
      }
   }
   else {
      // Parse: keyText^tabName/Remain
      // keyText 允許使用引號.
      // tabName = "key" or "key^tabName"
      hdr.KeyText_ = ParseKeyTextAndTabName(hdr.TabName_);
   }
   curr.OnTreeOp(hdr);
}
void SeedSearcher::PodHandler::operator()(const PodOpResult& resPod, PodOp* opPod) {
   if (opPod == nullptr) {
      this->Searcher_->RemainPath_.SetBegin(this->KeyPos_);
      this->Searcher_->OnError(resPod.OpResult_);
   }
   else if (TreeSP tree = this->Searcher_->ContinueTree(*opPod, *this->Tab_))
      ContinueSeedSearch(*tree, this->Searcher_);
   else {
      this->Searcher_->RemainPath_.SetBegin(this->KeyPos_);
      this->Searcher_->OnError(OpResult::not_found_sapling);
   }
}

fon9_API void StartSeedSearch(Tree& root, SeedSearcherSP searcher) {
   searcher->RemainPath_ = FilePath::RemovePathHead(searcher->RemainPath_);
   ContinueSeedSearch(root, std::move(searcher));
}

//--------------------------------------------------------------------------//

GridViewSearcher::GridViewSearcher(const StrView& path, const GridViewRequest& req, FnGridViewOp&& handler)
   : base{path}
   , Request_{req}
   , Handler_{std::move(handler)} {
   if (!IsTextBeginOrEnd(req.OrigKey_)) {
      this->OrigKey_.assign(req.OrigKey_);
      this->Request_.OrigKey_ = ToStrView(this->OrigKey_);
   }
}
void GridViewSearcher::OnError(OpResult opRes) {
   GridViewResult gvRes{opRes};
   this->Handler_(gvRes);
}
void GridViewSearcher::OnFoundTree(TreeOp& opTree) {
   if (this->TabIndex_ >= 0)
      this->Request_.Tab_ = opTree.Tree_.LayoutSP_->GetTab(static_cast<size_t>(this->TabIndex_));
   else if (!this->TabName_.empty())
      this->Request_.Tab_ = opTree.Tree_.LayoutSP_->GetTab(ToStrView(this->TabName_));
   opTree.GridView(this->Request_, std::move(this->Handler_));
}

//--------------------------------------------------------------------------//

void RemoveSeedSearcher::OnError(OpResult opRes) {
   PodRemoveResult res{opRes, StrView{this->OrigPath_.begin(), this->RemainPath_.begin()}};
   this->Handler_(res);
}
void RemoveSeedSearcher::OnFoundTree(TreeOp&) {
   this->OnError(OpResult::not_supported_remove_pod);
}
void RemoveSeedSearcher::ContinuePod(TreeOp& opTree, StrView keyText, StrView tabName) {
   this->ContinuePodForRemove(opTree, keyText, tabName, this->Handler_);
}
void SeedSearcher::ContinuePodForRemove(TreeOp& opTree, StrView keyText, StrView tabName, FnPodRemoved& removedHandler) {
   if (!this->RemainPath_.empty())
      this->SeedSearcher::ContinuePod(opTree, keyText, tabName);
   else {
      Tab* tab;
      if (tabName.empty())
         tab = nullptr;
      else if ((tab = opTree.Tree_.LayoutSP_->GetTab(tabName)) == nullptr) {
         this->RemainPath_.SetBegin(tabName.begin());
         this->OnError(OpResult::not_found_tab);
         return;
      }
      this->RemainPath_.SetBegin(keyText.begin());
      this->OnBeforeRemove(opTree, keyText, tab);
      opTree.Remove(keyText, tab, [this, &removedHandler](const PodRemoveResult& res) {
         if (res.OpResult_ == OpResult::removed_pod || res.OpResult_ == OpResult::removed_seed)
            removedHandler(res);
         else
            this->OnError(res.OpResult_);
         this->OnAfterRemove(res);
      });
   }
}
void SeedSearcher::OnBeforeRemove(TreeOp&, StrView, Tab*) {
}
void SeedSearcher::OnAfterRemove(const PodRemoveResult&) {
}

//--------------------------------------------------------------------------//

void WriteSeedSearcher::OnFoundTree(TreeOp& opTree) {
   (void)opTree;
   this->OnError(OpResult::not_supported_write);
}
void WriteSeedSearcher::OnLastStep(TreeOp& opTree, StrView keyText, Tab& tab) {
   opTree.Add(keyText, [this, &tab, &keyText](const PodOpResult& res, PodOp* op) {
      if (op)
         op->BeginWrite(tab, std::bind(&WriteSeedSearcher::OnBeginWrite, this, std::placeholders::_1, std::placeholders::_2));
      else {
         this->RemainPath_.SetBegin(keyText.begin());
         this->OnError(res.OpResult_);
      }
   });
}

void PutFieldSearcher::OnFieldValueChanged(const SeedOpResult& res, const RawWr& wr, const Field& fld) {
   (void)res; (void)wr; (void)fld;
}
void PutFieldSearcher::OnBeginWrite(const SeedOpResult& res, const RawWr* wr) {
   if (wr == nullptr)
      this->OnError(res.OpResult_);
   else {
      const Field* fld = (this->FieldIndex_ >= 0
                          ? res.Tab_->Fields_.Get(static_cast<size_t>(this->FieldIndex_))
                          : res.Tab_->Fields_.Get(ToStrView(this->FieldName_)));
      if (fld == nullptr)
         this->OnError(OpResult::not_found_field);
      else {
         fld->StrToCell(*wr, ToStrView(this->FieldValue_));
         this->OnFieldValueChanged(res, *wr, *fld);
      }
   }
}

} } // namespaces
