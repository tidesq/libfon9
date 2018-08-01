// \file fon9/seed/SeedSearcher.cpp
// \author fonwinz@gmail.com
#include "fon9/seed/SeedSearcher.hpp"
#include "fon9/seed/Tree.hpp"
#include "fon9/FilePath.hpp"

namespace fon9 { namespace seed {

SeedSearcher::~SeedSearcher() {
}
void SeedSearcher::OnFoundSeed(TreeOp& opTree, StrView keyText, Tab& tab) {
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
         this->OnFoundSeed(opTree, keyText, *tab);
      else
         this->ContinueSeed(opTree, keyText, *tab);
   }
   else {
      this->RemainPath_.SetBegin(tabName.empty() ? keyText.begin() : (tabName.begin() - 1));
      this->OnError(OpResult::not_found_tab);
   }
}

//--------------------------------------------------------------------------//

static void ContinueSeedSearch(Tree& curr, SeedSearcherSP searcher) {
   // 如果現在是 Thread-A.
   curr.OnTreeOp([searcher](const TreeOpResult& resTree, TreeOp* opTree) {
      // 此時這裡可能仍在 Thread-A, 但也可能在 Thread-B.
      if (opTree == nullptr)
         searcher->OnError(resTree.OpResult_);
      else if (searcher->RemainPath_.empty())
         searcher->OnFoundTree(*opTree);
      else {
         // Parse: keyText^tabName/Remain
         // keyText 允許使用引號.
         StrView tabName = FetchField(searcher->RemainPath_, '/', StrBrArg::Quotation_);
         StrView keyText = FetchFirstBrNoTrim(tabName, StrBrArg::Quotation_);
         if(keyText.begin() == nullptr)
            keyText = StrFetchNoTrim(tabName, '^');
         else // 移除引號之後, '^' 之前的字元:
            StrFetchNoTrim(tabName, '^');
         searcher->RemainPath_ = FilePath::RemovePathHead(searcher->RemainPath_);
         searcher->ContinuePod(*opTree, keyText, tabName);
      }
   });
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
   : SeedSearcher{path}
   , Request_{req}
   , Handler_{std::move(handler)} {
   if (!TreeOp::IsTextBegin(req.OrigKey_) && !TreeOp::IsTextEnd(req.OrigKey_)) {
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
void RemoveSeedSearcher::ContinuePod(TreeOp& opTree, fon9::StrView keyText, fon9::StrView tabName) {
   if (!this->RemainPath_.empty())
      base::ContinuePod(opTree, keyText, tabName);
   else {
      Tab* tab;
      if (tabName.empty())
         tab = nullptr;
      else if ((tab = opTree.Tree_.LayoutSP_->GetTab(tabName)) == nullptr) {
         this->OnError(OpResult::not_found_tab);
         return;
      }
      opTree.Remove(keyText, tab, std::move(this->Handler_));
   }
}

//--------------------------------------------------------------------------//

void WriteSeedSearcher::OnFoundTree(TreeOp& opTree) {
   (void)opTree;
   this->OnError(OpResult::not_supported_write);
}
void WriteSeedSearcher::OnFoundSeed(TreeOp& opTree, StrView keyText, Tab& tab) {
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
