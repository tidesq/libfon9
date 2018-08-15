// \file fon9/Tree_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "fon9/seed/Tree.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/seed/PodOp.hpp"
#include "fon9/LevelArray.hpp"
#include "fon9/TestTools.hpp"
#include "fon9/Timer.hpp"
#include <map>

//--------------------------------------------------------------------------//

#define kSPL   "\t"

// 因為在 64 位元系統裡面, 保證在 23 bytes 之內不用分配記憶體.
// 而商品 Id、姓名之類的字串，一般而言也不會太長.
using ShortStr = fon9::CharVector;

fon9_WARN_DISABLE_PADDING;
using Qty = uint32_t;
using Amt = uint64_t;
struct Bal {
   Qty   GnQty_{0};
   Qty   CrQty_{0};
   Qty   DbQty_{0};
   Amt   CrAmt_{0};
   Amt   DbAmt_{0};
};

struct BSVol {
   Qty   BuyQty_{0};
   Qty   SellQty_{0};
   Amt   BuyAmt_{0};
   Amt   SellAmt_{0};
};
struct TrVol {
   BSVol Order_;
   BSVol Match_;
};
/// 今日交易狀況.
struct TVol {
   TrVol Gn_;
   TrVol Cr_;
   TrVol Db_;
};
/// 因為每天 [Ivac-TVol] 的數量不會太多,
/// 所以只在有需要時才建立 TVol.
using TVolSP = std::unique_ptr<TVol>;

struct IvacSymb {
   fon9_NON_COPYABLE(IvacSymb);
   IvacSymb(IvacSymb&&) = default;
   IvacSymb() = default;
   Bal      Bal_;
   TVolSP   VolSP_;
};

using SymbId = ShortStr;
using IvacSymbMap = std::map<SymbId, IvacSymb>;
IvacSymbMap::iterator ContainerLowerBound(IvacSymbMap& c, const fon9::StrView& k) {
   return c.lower_bound(SymbId::MakeRef(k));
}
IvacSymbMap::iterator ContainerFind(IvacSymbMap& c, const fon9::StrView& k) {
   return c.find(SymbId::MakeRef(k));
}

struct IvacBasic {
   ShortStr Name_;
   Amt      BSLimit_{0};
   Amt      CrLimit_{0};
   Amt      DbLimit_{0};
};

using IvacNo = uint32_t;
constexpr IvacNo kMaxIvacNoChk = 999999;
constexpr IvacNo kEndIvacNoChk = kMaxIvacNoChk + 1;
constexpr IvacNo kInvalidIvacNo = static_cast<IvacNo>(-1);
struct Ivac {
   IvacNo      IvacNo_{kInvalidIvacNo}; // 包含檢查碼的帳號.
   IvacBasic   Basic_;
   IvacSymbMap Symbs_;
};

// Key(IvacNo) = 不含檢查碼.
using IvacMap = fon9::LevelArray<IvacNo, Ivac, 3>;
struct Brk {
   fon9_NON_COPYABLE(Brk);
   Brk(Brk&&) = default;
   Brk() = default;
   IvacMap  Ivacs_;
};

using BrkId = fon9::CharAry<4>;
using BrkMap = std::map<BrkId, Brk>;
fon9_WARN_POP;

//--------------------------------------------------------------------------//
/*
// BrkTree(Key=BrkNo)
// |
// +- 9900
// |         ________
// |  (Key) / Tab[0] \
// }  BrkNo ^ Ivacs
// +- 9901    .(Sapling)
// |          |          ________ ________
// |          |  (Key)  / Tab[0] \ Tab[1] \
// |          |  IvacNo ^ Basic  ^ Symbs
// |          +- (10      (Seed)   (Seed) ) - (Pod)
// |          |                    .(Sapling)
// |          |                    |          ________ ________
// |          |                    |  (Key)  / Tab[0] \ Tab[1] \
// |          |                    |  SymbId ^ Bal    ^ TVal
// |          |                    +- (1101    (Seed)   (Seed) ) - (Pod)
// |          |                    +- (2317    (Seed)   (Seed) ) - (Pod)
// |          |                    \- (2330    (Seed)   (Seed) ) - (Pod)
// |          |
// |          +- (21      (Seed)   (Seed) ) - (Pod)
// |          \- (32      (Seed)   (Seed) ) - (Pod)
// +- 9902
// +- 9903
// \- ...
*/
//--------------------------------------------------------------------------//
class IvacSymbTree : public fon9::seed::Tree {
   fon9_NON_COPY_NON_MOVE(IvacSymbTree);
   using base = fon9::seed::Tree;
   using Pod = IvacSymbMap::value_type;
   static fon9::seed::TabSP MakeTabBal() {
      fon9::seed::Fields flds;
      flds.Add(fon9_MakeField(fon9::Named{"GnQty"}, Pod, second.Bal_.GnQty_));
      flds.Add(fon9_MakeField(fon9::Named{"CrQty"}, Pod, second.Bal_.CrQty_));
      flds.Add(fon9_MakeField(fon9::Named{"DbQty"}, Pod, second.Bal_.DbQty_));
      flds.Add(fon9_MakeField(fon9::Named{"CrAmt"}, Pod, second.Bal_.CrAmt_));
      flds.Add(fon9_MakeField(fon9::Named{"DbAmt"}, Pod, second.Bal_.DbAmt_));
      return new fon9::seed::Tab{fon9::Named{"Bal"}, std::move(flds)};
   }
   static fon9::seed::TabSP MakeTabVol() {
      fon9::seed::Fields flds;
      // 為了簡化測試, 所以僅建立 Gn_ 欄位.
      flds.Add(fon9_MakeField(fon9::Named{"GnOrderBuyQty"},  TVol, Gn_.Order_.BuyQty_));
      flds.Add(fon9_MakeField(fon9::Named{"GnOrderSellQty"}, TVol, Gn_.Order_.SellQty_));
      flds.Add(fon9_MakeField(fon9::Named{"GnOrderBuyAmt"},  TVol, Gn_.Order_.BuyAmt_));
      flds.Add(fon9_MakeField(fon9::Named{"GnOrderSellAmt"}, TVol, Gn_.Order_.SellAmt_));
      flds.Add(fon9_MakeField(fon9::Named{"GnMatchBuyQty"},  TVol, Gn_.Match_.BuyQty_));
      flds.Add(fon9_MakeField(fon9::Named{"GnMatchSellQty"}, TVol, Gn_.Match_.SellQty_));
      flds.Add(fon9_MakeField(fon9::Named{"GnMatchBuyAmt"},  TVol, Gn_.Match_.BuyAmt_));
      flds.Add(fon9_MakeField(fon9::Named{"GnMatchSellAmt"}, TVol, Gn_.Match_.SellAmt_));
      return new fon9::seed::Tab{fon9::Named{"TVol"}, std::move(flds)};
   }
   static fon9::seed::FieldSP MakeKeyField() {
      fon9::seed::FieldSP res{fon9_MakeField(fon9::Named{"SymbId"}, Pod, first)};
      res->Source_ = fon9::seed::FieldSource::UserDefine;
      return res;
   }
public:
   IvacSymbMap& Symbs_;

   static constexpr size_t kTabBal = 0;
   static constexpr size_t kTabVol = 1;

   IvacSymbTree(IvacSymbMap& symbs)
      : base(new fon9::seed::LayoutN(MakeKeyField(), MakeTabBal(),MakeTabVol()),
             fon9::seed::TreeFlag::Shadow | fon9::seed::TreeFlag::AddableRemovable)
      , Symbs_(symbs) {
   }

   fon9_MSC_WARN_DISABLE(4265);//class has virtual functions, but destructor is not virtual
   class PodOp : public fon9::seed::PodOpDefault {
      fon9_NON_COPY_NON_MOVE(PodOp);
      using base = fon9::seed::PodOpDefault;
   public:
      Pod&  Pod_;
      PodOp(Pod& pod, fon9::seed::Tree& sender, fon9::seed::OpResult res, const fon9::StrView& key)
         : base{sender, res, key}
         , Pod_(pod) {
      }
      template <class RawRW, class FnOp>
      void BeginRW(fon9::seed::Tab& tab, FnOp&& fnCallback) {
         assert(this->Sender_->LayoutSP_->GetTab(static_cast<size_t>(tab.GetIndex())) == &tab);
         this->Tab_ = &tab;
         switch (tab.GetIndex()) {
         default:
            this->OpResult_ = fon9::seed::OpResult::not_found_tab;
            fnCallback(*this, nullptr);
            break;
         case kTabVol:
            if (TVol* vol = this->Pod_.second.VolSP_.get()) {
               RawRW op{*vol};
               this->OpResult_ = fon9::seed::OpResult::no_error;
               fnCallback(*this, &op);
            }
            else {
               this->OpResult_ = fon9::seed::OpResult::not_found_seed;
               fnCallback(*this, nullptr);
            }
            break;
         case kTabBal:
            RawRW op{this->Pod_};
            this->OpResult_ = fon9::seed::OpResult::no_error;
            fnCallback(*this, &op);
            break;
         }
      }
      void BeginRead(fon9::seed::Tab& tab, fon9::seed::FnReadOp fnCallback) override {
         this->BeginRW<fon9::seed::SimpleRawRd>(tab, std::move(fnCallback));
      }
      void BeginWrite(fon9::seed::Tab& tab, fon9::seed::FnWriteOp fnCallback) override {
         if (tab.GetIndex() == kTabVol && !this->Pod_.second.VolSP_)
            this->Pod_.second.VolSP_.reset(new TVol);
         this->BeginRW<fon9::seed::SimpleRawWr>(tab, std::move(fnCallback));
      }
   };
   struct TreeOp : public fon9::seed::TreeOp {
      fon9_NON_COPY_NON_MOVE(TreeOp);
      using base = fon9::seed::TreeOp;
      TreeOp(IvacSymbTree& tree) : base(tree) {
      }
      void Get(fon9::StrView strKeyText, fon9::seed::FnPodOp fnCallback) override {
         GetPod(*static_cast<IvacSymbTree*>(&this->Tree_),
                static_cast<IvacSymbTree*>(&this->Tree_)->Symbs_,
                this->GetIteratorForPod(static_cast<IvacSymbTree*>(&this->Tree_)->Symbs_, strKeyText),
                strKeyText,
                std::move(fnCallback));
      }
      void GridView(const fon9::seed::GridViewRequest& req, fon9::seed::FnGridViewOp fnCallback) override {
         IvacSymbMap::iterator      istart = this->GetIteratorForGv(static_cast<IvacSymbTree*>(&this->Tree_)->Symbs_, req.OrigKey_);
         fon9::seed::GridViewResult res{this->Tree_, req.Tab_};
         if (req.Tab_ && req.Tab_->GetIndex() == kTabVol) {
            fon9::seed::MakeGridView(static_cast<IvacSymbTree*>(&this->Tree_)->Symbs_, istart, req, res,
                                     [](IvacSymbMap::iterator ivalue, fon9::seed::Tab* tab, fon9::RevBuffer& rbuf) {
               if (tab && ivalue->second.VolSP_)
                  FieldsCellRevPrint(tab->Fields_, fon9::seed::SimpleRawRd{*ivalue->second.VolSP_},
                                     rbuf, fon9::seed::GridViewResult::kCellSplitter);
               RevPrint(rbuf, ivalue->first);
            });
         }
         else
            fon9::seed::MakeGridView(static_cast<IvacSymbTree*>(&this->Tree_)->Symbs_, istart, req, res,
                                     &fon9::seed::SimpleMakeRowView<IvacSymbMap::iterator>);
         fnCallback(res);
      }

      void Add(fon9::StrView strKeyText, fon9::seed::FnPodOp fnCallback) override {
         auto   i = static_cast<IvacSymbTree*>(&this->Tree_)->Symbs_.emplace(Pod{SymbId::MakeRef(strKeyText), IvacSymb()});
         PodOp  op{*i.first, this->Tree_,
            (i.second ? fon9::seed::OpResult::no_error : fon9::seed::OpResult::key_exists),
            strKeyText};
         fnCallback(op, &op);
      }
      void Remove(fon9::StrView strKeyText, fon9::seed::Tab* tab, fon9::seed::FnPodRemoved fnCallback) override {
         RemovePod(this->Tree_,
                   static_cast<IvacSymbTree*>(&this->Tree_)->Symbs_,
                   SymbId::MakeRef(strKeyText),
                   strKeyText,
                   tab,
                   std::move(fnCallback));
      }
   };
   fon9_MSC_WARN_POP;

   virtual void OnTreeOp(fon9::seed::FnTreeOp fnCallback) override {
      TreeOp op{*this};
      fnCallback(fon9::seed::TreeOpResult{this, fon9::seed::OpResult::no_error}, &op);
   }
};
//--------------------------------------------------------------------------//
class IvacTree : public fon9::seed::Tree {
   fon9_NON_COPY_NON_MOVE(IvacTree);
   using base = fon9::seed::Tree;
   using Pod = IvacMap::value_type;
   static fon9::seed::TabSP MakeTabBasic() {
      fon9::seed::Fields flds;
      flds.Add(fon9_MakeField(fon9::Named{"Name"},     Pod, Basic_.Name_));
      flds.Add(fon9_MakeField(fon9::Named{"BSLimit"},  Pod, Basic_.BSLimit_));
      flds.Add(fon9_MakeField(fon9::Named{"CrLimit_"}, Pod, Basic_.CrLimit_));
      flds.Add(fon9_MakeField(fon9::Named{"DbLimit_"}, Pod, Basic_.DbLimit_));
      return new fon9::seed::Tab{fon9::Named{"Basic"}, std::move(flds)};
   }
   static fon9::seed::TabSP MakeTabSymbs() {
      fon9::seed::Fields flds;
      return new fon9::seed::Tab{fon9::Named{"Symbs"}, std::move(flds)};
   }

public:
   IvacMap& Ivacs_;

   static constexpr size_t kTabBasic = 0;
   static constexpr size_t kTabSymbs = 1;

   IvacTree(IvacMap& ivacs)
      : base(new fon9::seed::LayoutN(
                     fon9_MakeField(fon9::Named{"IvacNo"}, Pod, IvacNo_),
                     MakeTabBasic(),
                     MakeTabSymbs()),
             fon9::seed::TreeFlag::Shadow | fon9::seed::TreeFlag::AddableRemovable)
      , Ivacs_(ivacs) {
   }

   fon9_MSC_WARN_DISABLE(4265);//class has virtual functions, but destructor is not virtual
   class PodOp : public fon9::seed::PodOpDefault {
      fon9_NON_COPY_NON_MOVE(PodOp);
      using base = fon9::seed::PodOpDefault;
   public:
      Pod&  Pod_;
      PodOp(Pod& pod, fon9::seed::Tree& sender, fon9::seed::OpResult res, const fon9::StrView& key)
         : base{sender, res, key}
         , Pod_(pod) {
      }
      template <class RawRW, class FnOp>
      void BeginRW(fon9::seed::Tab& tab, FnOp&& fnCallback) {
         assert(this->Sender_->LayoutSP_->GetTab(static_cast<size_t>(tab.GetIndex())) == &tab);
         this->Tab_ = &tab;
         switch (tab.GetIndex()) {
         default:
            this->OpResult_ = fon9::seed::OpResult::not_found_tab;
            fnCallback(*this, nullptr);
            break;
         case kTabBasic:
         case kTabSymbs:
            RawRW op{this->Pod_};
            this->OpResult_ = fon9::seed::OpResult::no_error;
            fnCallback(*this, &op);
            break;
         }
      }
      void BeginRead(fon9::seed::Tab& tab, fon9::seed::FnReadOp fnCallback) override {
         this->BeginRW<fon9::seed::SimpleRawRd>(tab, std::move(fnCallback));
      }
      void BeginWrite(fon9::seed::Tab& tab, fon9::seed::FnWriteOp fnCallback) override {
         this->BeginRW<fon9::seed::SimpleRawWr>(tab, std::move(fnCallback));
      }
      fon9::seed::TreeSP GetSapling(fon9::seed::Tab& tab) override {
         (void)tab; assert(this->Sender_->LayoutSP_->GetTab(static_cast<size_t>(tab.GetIndex())) == &tab);
         if (tab.GetIndex() == kTabSymbs)
            return fon9::seed::TreeSP{new IvacSymbTree{this->Pod_.Symbs_}};
         return fon9::seed::TreeSP{};
      }
   };
   struct TreeOp : public fon9::seed::TreeOp {
      fon9_NON_COPY_NON_MOVE(TreeOp);
      using base = fon9::seed::TreeOp;
      TreeOp(IvacTree& tree) : base(tree) {
      }
      bool GetIvacNoChk(IvacNo& ivalue, fon9::StrView strKeyText) {
         if (strKeyText.begin() == kStrKeyText_Begin_)
            ivalue = 0;
         else if (strKeyText.begin() == kStrKeyText_End_)
            ivalue = kEndIvacNoChk;
         else {
            IvacNo key = fon9::StrTo(strKeyText, 0u);
            // StrToKey(); => fon9::seed::OpResult::key_format_error?
            ivalue = key / 10; // 移除檢查碼.
            if (ivalue >= kEndIvacNoChk)
               ivalue = kEndIvacNoChk;
         }
         return true;
      }
      void Get(fon9::StrView strKeyText, fon9::seed::FnPodOp fnCallback) override {
         IvacNo ivalue;
         if (!this->GetIvacNoChk(ivalue, strKeyText) || ivalue >= kEndIvacNoChk)
            fnCallback(fon9::seed::PodOpResult{this->Tree_, fon9::seed::OpResult::key_format_error, strKeyText}, nullptr);
         else {
            Pod*  pod = static_cast<IvacTree*>(&this->Tree_)->Ivacs_.Get(ivalue);
            if (pod == nullptr || pod->IvacNo_ == kInvalidIvacNo)
               fnCallback(fon9::seed::PodOpResult(this->Tree_, fon9::seed::OpResult::not_found_key, strKeyText), nullptr);
            else {
               PodOp op{*pod, this->Tree_, fon9::seed::OpResult::no_error, strKeyText};
               fnCallback(op, &op);
            }
         }
      }
      void Add(fon9::StrView strKeyText, fon9::seed::FnPodOp fnCallback) override {
         IvacNo ivacNo = fon9::StrTo(strKeyText, 0u); // StrToKey(); => fon9::seed::OpResult::key_format_error?
         // TODO: 判斷檢查碼是否正確, 如果不正確: not_found_key.
         IvacNo ivalue = ivacNo / 10; // 移除檢查碼.
         if (ivalue >= kEndIvacNoChk)
            fnCallback(fon9::seed::PodOpResult{this->Tree_, fon9::seed::OpResult::key_format_error, strKeyText}, nullptr);
         else {
            Pod&  pod = static_cast<IvacTree*>(&this->Tree_)->Ivacs_[ivalue];
            if (pod.IvacNo_ != kInvalidIvacNo && pod.IvacNo_ != ivacNo)
               fnCallback(fon9::seed::PodOpResult(this->Tree_, fon9::seed::OpResult::not_found_key, strKeyText), nullptr);
            else {
               pod.IvacNo_ = ivacNo;
               PodOp op{pod, this->Tree_, fon9::seed::OpResult::no_error, strKeyText};
               fnCallback(op, &op);
            }
         }
      }
      void GridView(const fon9::seed::GridViewRequest& req, fon9::seed::FnGridViewOp fnCallback) override {
         fon9::seed::GridViewResult res{this->Tree_, req.Tab_};
         IvacNo istart;
         if (this->GetIvacNoChk(istart, req.OrigKey_)) {
            constexpr unsigned maxDefaultCount = 100;
            IvacNo iend = (req.MaxRowCount_ != 0) ? kEndIvacNoChk
               : istart >= (kEndIvacNoChk - maxDefaultCount) ? kEndIvacNoChk
               : (istart + maxDefaultCount);
            fon9::seed::MakeGridViewArrayRange(istart, iend, req, res,
                                               [&res](IvacNo ivalue, fon9::seed::Tab* tab, fon9::RevBuffer& rbuf) {
               const Pod* pod = static_cast<IvacTree*>(res.Sender_)->Ivacs_.Get(ivalue);
               if (pod == nullptr || pod->IvacNo_ == kInvalidIvacNo)
                  return false;
               if (tab)
                  FieldsCellRevPrint(tab->Fields_, fon9::seed::SimpleRawRd{*pod}, rbuf, fon9::seed::GridViewResult::kCellSplitter);
               RevPrint(rbuf, pod->IvacNo_);
               return true;
            });
         }
         else
            res.OpResult_ = fon9::seed::OpResult::key_format_error;
         fnCallback(res);
      }
      void Remove(fon9::StrView strKeyText, fon9::seed::Tab* tab, fon9::seed::FnPodRemoved fnCallback) override {
         fon9::seed::PodRemoveResult res{this->Tree_, fon9::seed::OpResult::key_format_error, strKeyText, tab};
         IvacNo ivalue;
         if (this->GetIvacNoChk(ivalue, strKeyText)) {
            if (Pod* pod = static_cast<IvacTree*>(&this->Tree_)->Ivacs_.Get(ivalue)) {
               if (tab == nullptr) {
                  *pod = Pod{};
                  res.OpResult_ = fon9::seed::OpResult::removed_pod;
               }
               else {
                  switch (tab->GetIndex()) {
                  default:
                     res.OpResult_ = fon9::seed::OpResult::not_found_tab;
                     break;
                  case kTabBasic:
                     pod->Basic_ = IvacBasic{};
                     res.OpResult_ = fon9::seed::OpResult::removed_seed;
                     break;
                  case kTabSymbs:
                     pod->Symbs_ = IvacSymbMap{};
                     res.OpResult_ = fon9::seed::OpResult::removed_seed;
                     break;
                  }
               }
            }
            else
               res.OpResult_ = fon9::seed::OpResult::not_found_key;
         }
         fnCallback(res);
      }
   };
   fon9_MSC_WARN_POP;

   virtual void OnTreeOp(fon9::seed::FnTreeOp fnCallback) override {
      TreeOp op{*this};
      fnCallback(fon9::seed::TreeOpResult{this, fon9::seed::OpResult::no_error}, &op);
   }
};
//--------------------------------------------------------------------------//
class BrkTree : public fon9::seed::Tree {
   fon9_NON_COPY_NON_MOVE(BrkTree);
   using base = fon9::seed::Tree;
   using Pod = BrkMap::value_type;
   static fon9::seed::TabSP MakeTab() {
      fon9::seed::Fields flds;
      return new fon9::seed::Tab{fon9::Named{"Ivacs"}, std::move(flds)};
   }
public:
   BrkMap   BrkMap_;

   static constexpr size_t kTabIvacs = 0;

   BrkTree()
      : base(new fon9::seed::Layout1(fon9_MakeField(fon9::Named{"BrkNo"}, Pod, first), MakeTab()),
             fon9::seed::TreeFlag::AddableRemovable) {
   }

   fon9_MSC_WARN_DISABLE(4265);//class has virtual functions, but destructor is not virtual
   class PodOp : public fon9::seed::PodOpDefault {
      fon9_NON_COPY_NON_MOVE(PodOp);
      using base = fon9::seed::PodOpDefault;
   public:
      Pod&  Pod_;
      PodOp(Pod& pod, fon9::seed::Tree& sender, fon9::seed::OpResult res, const fon9::StrView& key)
         : base{sender, res, key}
         , Pod_(pod) {
      }
      fon9::seed::TreeSP GetSapling(fon9::seed::Tab& tab) override {
         (void)tab; assert(this->Sender_->LayoutSP_->GetTab(static_cast<size_t>(tab.GetIndex())) == &tab);
         return fon9::seed::TreeSP{new IvacTree{this->Pod_.second.Ivacs_}};
      }
   };
   struct TreeOp : public fon9::seed::TreeOp {
      fon9_NON_COPY_NON_MOVE(TreeOp);
      using base = fon9::seed::TreeOp;
      TreeOp(BrkTree& tree) : base(tree) {
      }
      void Get(fon9::StrView strKeyText, fon9::seed::FnPodOp fnCallback) override {
         auto& brkmap = static_cast<BrkTree*>(&this->Tree_)->BrkMap_;
         GetPod(*static_cast<BrkTree*>(&this->Tree_), brkmap,
                this->GetIteratorForPod(brkmap, strKeyText),
                strKeyText,
                std::move(fnCallback));
      }
      void GridView(const fon9::seed::GridViewRequest& req, fon9::seed::FnGridViewOp fnCallback) override {
         fon9::seed::GridViewResult res{this->Tree_, req.Tab_};
         auto&                      brkmap = static_cast<BrkTree*>(&this->Tree_)->BrkMap_;
         fon9::seed::MakeGridView(brkmap, this->GetIteratorForGv(brkmap, req.OrigKey_), req, res,
                                  &fon9::seed::SimpleMakeRowView<BrkMap::iterator>);
         fnCallback(res);
      }

      void Add(fon9::StrView strKeyText, fon9::seed::FnPodOp fnCallback) override {
         if (strKeyText.size() != sizeof(BrkId::Chars_)) {
            fnCallback(fon9::seed::PodOpResult{this->Tree_, fon9::seed::OpResult::key_format_error, strKeyText}, nullptr);
            return;
         }
         auto  i = static_cast<BrkTree*>(&this->Tree_)->BrkMap_.insert(Pod{strKeyText,Brk{}});
         PodOp op{*i.first, this->Tree_,
            (i.second ? fon9::seed::OpResult::no_error : fon9::seed::OpResult::key_exists),
            strKeyText};
         fnCallback(op, &op);
      }
      void Remove(fon9::StrView strKeyText, fon9::seed::Tab* tab, fon9::seed::FnPodRemoved fnCallback) override {
         RemovePod(this->Tree_,
                   static_cast<BrkTree*>(&this->Tree_)->BrkMap_,
                   strKeyText,
                   strKeyText,
                   tab,
                   std::move(fnCallback));
      }
   };
   fon9_MSC_WARN_POP;

   virtual void OnTreeOp(fon9::seed::FnTreeOp fnCallback) override {
      TreeOp op{*this};
      fnCallback(fon9::seed::TreeOpResult{this, fon9::seed::OpResult::no_error}, &op);
   }
};
//--------------------------------------------------------------------------//

void CheckGridView(fon9::seed::TreeOp* op, fon9::seed::Tab* tab, fon9::StrView expectResult) {
   const std::string& name = (tab ? tab->Name_ : op->Tree_.LayoutSP_->KeyField_->Name_);
   std::cout << '\n' << name;
   fon9::seed::GridViewRequest req{fon9::seed::TreeOp::TextBegin()};
   req.Tab_ = tab;
   op->GridView(req, [expectResult](fon9::seed::GridViewResult& res) {
      std::cout << "|RowCount=" << res.RowCount_;
      std::cout << "|ContainerSize=";
      if (res.ContainerSize_ != res.kNotSupported)
         std::cout << res.ContainerSize_;
      else
         std::cout << "NotSupported";
      std::cout << '\n' << res.GridView_ << '\n';
      if (expectResult != fon9::ToStrView(res.GridView_)) {
         std::cout << "[ERROR] Expect is:\n" << expectResult.begin() << std::endl;
         abort();
      }
   });
}

int main(int argc, char** args) {
   (void)argc; (void)args;
#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
   //_CrtSetBreakAlloc(176);
#endif
   fon9::AutoPrintTestInfo utinfo{"Tree/TreeOp"};
   fon9::GetDefaultTimerThread();
   std::this_thread::sleep_for(std::chrono::milliseconds{10});

   fon9::seed::TreeSPT<BrkTree>  brks{new BrkTree};
   fon9::seed::TreeSP            b9901Ivacs;

   fon9_GCC_WARN_DISABLE("-Wshadow");
   brks->OnTreeOp([&b9901Ivacs](const fon9::seed::TreeOpResult&, fon9::seed::TreeOp* op) {
      op->Add("9900", [](const fon9::seed::PodOpResult&, fon9::seed::PodOp*) {});
      op->Add("9901", [&b9901Ivacs](const fon9::seed::PodOpResult& r, fon9::seed::PodOp* op) {
         b9901Ivacs = op->MakeSapling(*r.Sender_->LayoutSP_->GetTab(0));
      });
      op->Add("9902", [](const fon9::seed::PodOpResult&, fon9::seed::PodOp*) {});
      op->Add("9903", [](const fon9::seed::PodOpResult&, fon9::seed::PodOp*) {});
   
      CheckGridView(op, nullptr, "9900\n" "9901\n" "9902\n" "9903");
   });
   
   static const char* ivacDats[][5] = {
      {"10", "Show", "90000", "91000", "92000"},
      {"21", "Fonwin", "10000", "", ""},
      {"32", "Tony", "20000", "21000", "22000"},
   };
   b9901Ivacs->OnTreeOp([](const fon9::seed::TreeOpResult& r, fon9::seed::TreeOp* op) {
      fon9::seed::Tab* tab = r.Sender_->LayoutSP_->GetTab(0);
      for (auto dats : ivacDats) {
         op->Add(fon9::StrView_cstr(dats[0]), [tab, dats](const fon9::seed::PodOpResult&, fon9::seed::PodOp* op) {
            op->BeginWrite(*tab, [dats](const fon9::seed::SeedOpResult& r, const fon9::seed::RawWr* wr) {
               size_t fldidx = 0;
               while (const fon9::seed::Field* fld = r.Tab_->Fields_.Get(fldidx++))
                  fld->StrToCell(*wr, fon9::StrView_cstr(dats[fldidx]));
            });
         });
      }
      CheckGridView(op, tab,
                    "10" kSPL "Show"   kSPL "90000" kSPL "91000" kSPL "92000" "\n"
                    "21" kSPL "Fonwin" kSPL "10000" kSPL "0"     kSPL "0" "\n"
                    "32" kSPL "Tony"   kSPL "20000" kSPL "21000" kSPL "22000" // 最後一筆尾端無換行!
                  );

   });

   struct BalValues { // GnQty,CrQty,DbQty,CrAmt,DbAmt
      fon9::seed::FieldNumberT Vals_[5];
   };
   struct VolValues {
      // GnOrderBuyQty,GnOrderSellQty,GnOrderBuyAmt,GnOrderSellAmt,GnMatchBuyQty,GnMatchSellQty,GnMatchBuyAmt,GnMatchSellAmt
      const char* Vals_[8];
   };
   struct IvacSymb {
      const char* SymbId_;
      BalValues   Bal_;
      VolValues   Vol_;
   };
   static IvacSymb ivacSymbs[] = {
      {"2317",{1,2,3,4,5},{"6","7","8","9"}},
      {"2330",{5,4,3,2,1},{"9","8","7","6"}},
   };
   b9901Ivacs->OnTreeOp([](const fon9::seed::TreeOpResult&, fon9::seed::TreeOp* op) {
      op->Get(/*fon9::seed::TreeOp::TextBegin()*/"10", [](const fon9::seed::PodOpResult& r, fon9::seed::PodOp* op) {
         auto symbTree = op->MakeSapling(*r.Sender_->LayoutSP_->GetTab("Symbs"));
         symbTree->OnTreeOp([](const fon9::seed::TreeOpResult& r, fon9::seed::TreeOp* op) {
            for (auto& dats : ivacSymbs) {
               op->Add(fon9::StrView_cstr(dats.SymbId_), [&dats](const fon9::seed::PodOpResult& r, fon9::seed::PodOp* op) {
                  fon9::seed::Tab* tab = r.Sender_->LayoutSP_->GetTab(IvacSymbTree::kTabBal);
                  op->BeginWrite(*tab, [&dats](const fon9::seed::SeedOpResult& r, const fon9::seed::RawWr* wr) {
                     size_t fldidx = 0;
                     while (const fon9::seed::Field* fld = r.Tab_->Fields_.Get(fldidx))
                        fld->PutNumber(*wr, dats.Bal_.Vals_[fldidx++], 0);
                  });
                  tab = r.Sender_->LayoutSP_->GetTab(IvacSymbTree::kTabVol);
                  op->BeginWrite(*tab, [&dats](const fon9::seed::SeedOpResult& r, const fon9::seed::RawWr* wr) {
                     size_t fldidx = 0;
                     while (const fon9::seed::Field* fld = r.Tab_->Fields_.Get(fldidx))
                        fld->StrToCell(*wr, fon9::StrView_cstr(dats.Vol_.Vals_[fldidx++]));
                  });
               });
            }
            op->Add("1101", [](const fon9::seed::PodOpResult&, fon9::seed::PodOp*) {});

            CheckGridView(op, r.Sender_->LayoutSP_->GetTab(IvacSymbTree::kTabBal),
                          "1101" kSPL "0" kSPL "0" kSPL "0" kSPL "0" kSPL "0" "\n"
                          "2317" kSPL "1" kSPL "2" kSPL "3" kSPL "4" kSPL "5" "\n"
                          "2330" kSPL "5" kSPL "4" kSPL "3" kSPL "2" kSPL "1" // 最後一筆尾端無換行!
                          );
            CheckGridView(op, r.Sender_->LayoutSP_->GetTab(IvacSymbTree::kTabVol),
                          "1101" /* 空資料 */ "\n"
                          "2317" kSPL "6" kSPL "7" kSPL "8" kSPL "9" kSPL "0" kSPL "0" kSPL "0" kSPL "0" "\n"
                          "2330" kSPL "9" kSPL "8" kSPL "7" kSPL "6" kSPL "0" kSPL "0" kSPL "0" kSPL "0"
                          );
         });
      });
   });
   fon9_GCC_WARN_POP;

   std::cout << std::endl;
}
