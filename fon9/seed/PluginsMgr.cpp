// \file fon9/seed/PluginsMgr.cpp
// \author fonwinz@gmail.com
#include "fon9/seed/PluginsMgr.hpp"
#include "fon9/seed/Plugins.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/seed/ConfigGridView.hpp"
#include "fon9/AQueue.hpp"
#include "fon9/DefaultThreadPool.hpp"
#include "fon9/ConfigFileBinder.hpp"

namespace fon9 { namespace seed {

fon9_WARN_DISABLE_PADDING;
struct PluginsMgr::PluginsRec : public PluginsHolder {
   fon9_NON_COPYABLE(PluginsRec);

   const PluginsTreeSP  Owner_;
   const CharVector     Id_;
   CharVector           FileName_;
   CharVector           EntryName_;
   CharVector           Description_;
   CharVector           Args_;
   CharVector           Status_;
   EnabledYN            Enabled_{};

   PluginsRec(PluginsTreeSP owner, StrView id);
   ~PluginsRec() {
      this->StopPlugins();
   }
   void InThr_StartPlugins();
   void InThr_StopPlugins();
   void InThr_OnChanged();
   void SetPluginStImpl(std::string stmsg) override;
};
using PluginsRec = PluginsMgr::PluginsRec;
using PluginsRecSP = intrusive_ptr<PluginsRec>;
struct PluginsIdCmp {
   inline bool operator()(const StrView& lhs, const PluginsRecSP& rhs) const {
      return lhs < ToStrView(rhs->Id_);
   }
   inline bool operator()(const PluginsRecSP& lhs, const StrView& rhs) const {
      return ToStrView(lhs->Id_) < rhs;
   }
   inline bool operator()(const PluginsRecSP& lhs, const PluginsRecSP& rhs) const {
      return lhs->Id_ < rhs->Id_;
   }
};
using PluginsRecs = SortedVectorSet<PluginsRecSP, PluginsIdCmp>;
//--------------------------------------------------------------------------//
static LayoutSP MakePluginsLayout() {
   Fields flds;
   flds.Add(fon9_MakeField(Named{"Enabled"}, PluginsRec, Enabled_));
   flds.Add(fon9_MakeField(Named{"FileName"}, PluginsRec, FileName_));
   flds.Add(fon9_MakeField(Named{"EntryName"}, PluginsRec, EntryName_));
   flds.Add(fon9_MakeField_const(Named{"Description"}, PluginsRec, Description_));
   flds.Add(fon9_MakeField(Named{"Args"}, PluginsRec, Args_));
   flds.Add(fon9_MakeField_const(Named{"Status"}, PluginsRec, Status_));
   return new Layout1(fon9_MakeField(Named{"Id"}, PluginsRec, Id_),
                      new Tab{Named{"Plugins"}, std::move(flds), TabFlag::NoSapling | TabFlag::Writable},
                      TreeFlag::AddableRemovable);
}

struct PluginsMgr::PluginsTree : public Tree {
   fon9_NON_COPY_NON_MOVE(PluginsTree);
   using base = Tree;
   using Task = std::function<void()>;
   struct TaskInvoker {
      template <class Locker>
      void MakeCallForWork(Locker& locker) {
         locker.unlock();
         this->MakeCallForWork();
      }
      void MakeCallForWork();
      void Invoke(Task& task) {
         task();
      }
      Task MakeWaiterTask(Task& task, CountDownLatch& waiter) {
        return Task{[&]() {
              this->Invoke(task);
              waiter.ForceWakeUp();
           }};
      }
   };
   using TaskQu = AQueue<Task, TaskInvoker>;
   PluginsMgr&       PluginsMgr_;
   TaskQu            TaskQu_;
   PluginsRecs       PluginsRecs_;
   const MaTreeSP    Root_;
   ConfigFileBinder  ConfigFileBinder_;

   fon9_MSC_WARN_DISABLE(4265 /* class has virtual functions, but destructor is not virtual. */);
   struct PodOp : public PodOpDefault {
      fon9_NON_COPY_NON_MOVE(PodOp);
      using base = PodOpDefault;
      PluginsRec& Rec_;
      bool        IsChanged_{false};
      PodOp(PluginsRec& rec, Tree& sender)
         : base(sender, OpResult::no_error, ToStrView(rec.Id_))
         , Rec_(rec) {
      }
      void BeginRead(Tab& tab, FnReadOp fnCallback) override {
         this->BeginRW(tab, std::move(fnCallback), SimpleRawRd{this->Rec_});
      }
      void BeginWrite(Tab& tab, FnWriteOp fnCallback) override {
         this->BeginRW(tab, std::move(fnCallback), SimpleRawWr{this->Rec_});
         this->IsChanged_ = true;
      }
      void OnSeedCommand(Tab* tab, StrView cmdln, FnCommandResultHandler resHandler) override {
         if (auto desc = this->Rec_.GetPluginsDesc()) {
            if (desc->FnCommand_)
               return desc->FnCommand_(this->Rec_, cmdln, std::move(resHandler));
         }
         base::OnSeedCommand(tab, cmdln, std::move(resHandler));
      }
   };
   struct TreeOp : public seed::TreeOp {
      fon9_NON_COPY_NON_MOVE(TreeOp);
      using base = seed::TreeOp;
      TreeOp(PluginsTree& tree) : base{tree} {
      }
      static void MakePluginsRecView(PluginsRecs::iterator ivalue, Tab* tab, RevBuffer& rbuf) {
         PluginsRec& rec = **ivalue;
         if (tab)
            FieldsCellRevPrint(tab->Fields_, SimpleRawRd{rec}, rbuf, GridViewResult::kCellSplitter);
         RevPrint(rbuf, rec.Id_);
      }
      void GridView(const GridViewRequest& req, FnGridViewOp fnCallback) override {
         GridViewResult res{this->Tree_, req.Tab_};
         PluginsRecs&   container = static_cast<PluginsTree*>(&this->Tree_)->PluginsRecs_;
         MakeGridView(container, GetIteratorForGv(container, req.OrigKey_),
                      req, res, &MakePluginsRecView);
         fnCallback(res);
      }
      void Remove(StrView strKeyText, Tab* tab, FnPodRemoved fnCallback) override {
         PodRemoveResult res{this->Tree_, OpResult::not_found_key, strKeyText, tab};
         PluginsRecs&    container = static_cast<PluginsTree*>(&this->Tree_)->PluginsRecs_;
         auto            ifind = container.find(strKeyText);
         if (ifind != container.end()) {
            ifind->get()->InThr_StopPlugins();
            container.erase(ifind);
            res.OpResult_ = OpResult::removed_pod;
            static_cast<PluginsTree*>(&this->Tree_)->InThr_OnConfigChanged();
         }
         fnCallback(res);
      }
      void OnPodOp(PluginsRec& rec, FnPodOp&& fnCallback, bool isForceWrite = false) {
         PodOp op{rec, this->Tree_};
         fnCallback(op, &op);
         if (op.IsChanged_ || isForceWrite)
            rec.InThr_OnChanged();
      }
      void Get(StrView strKeyText, FnPodOp fnCallback) override {
         PluginsRecs& container = static_cast<PluginsTree*>(&this->Tree_)->PluginsRecs_;
         auto         ifind = container.find(strKeyText);
         if (ifind != container.end())
            this->OnPodOp(**ifind, std::move(fnCallback));
         else
            fnCallback(PodOpResult{this->Tree_, OpResult::not_found_key, strKeyText}, nullptr);
      }
      void Add(StrView strKeyText, FnPodOp fnCallback) override {
         if (IsTextBeginOrEnd(strKeyText)) {
            fnCallback(PodOpResult{this->Tree_, OpResult::not_found_key, strKeyText}, nullptr);
            return;
         }
         PluginsRecs& container = static_cast<PluginsTree*>(&this->Tree_)->PluginsRecs_;
         auto         ifind = container.find(strKeyText);
         bool         isForceWrite = false;
         if (ifind == container.end()) {
            isForceWrite = true;
            ifind = container.insert(PluginsRecSP{new PluginsRec(static_cast<PluginsTree*>(&this->Tree_), strKeyText)}).first;
         }
         this->OnPodOp(**ifind, std::move(fnCallback), isForceWrite);
      }
   };
   fon9_MSC_WARN_POP;

   PluginsTree(PluginsMgr& pluginsMgr, MaTreeSP&& root)
      : base{MakePluginsLayout()}
      , PluginsMgr_(pluginsMgr)
      , Root_{std::move(root)} {
   }
   void OnTreeOp(FnTreeOp fnCallback) override {
      this->TaskQu_.AddTask([this, fnCallback]() {
         TreeOp op{*this};
         fnCallback(TreeOpResult{this, OpResult::no_error}, &op);
      });
   }
   void OnParentSeedClear() override {
      this->TaskQu_.AddTask([this]() {
         for (auto& irec : this->PluginsRecs_)
            irec->InThr_StopPlugins();
         this->PluginsRecs_.clear();
      });
   }

   void InThr_OnConfigChanged() {
      assert(this->TaskQu_.InTakingCallThread());
      if (!this->ConfigFileBinder_.HasBinding())
         return;
      auto          iend = this->PluginsRecs_.end();
      auto          ibeg = this->PluginsRecs_.begin();
      const Tab*    tab = this->LayoutSP_->GetTab(0);
      const Fields& flds = tab->Fields_;
      RevBufferList rbuf{128};
      while (ibeg != iend) {
         const PluginsRec& rec = **--iend;
         RevPrintConfigFieldValues(rbuf, flds, SimpleRawRd{rec});
         RevPrint(rbuf, *fon9_kCSTR_ROWSPL, rec.Id_);
      }
      RevPrintConfigFieldNames(rbuf, flds, &this->LayoutSP_->KeyField_->Name_);
      std::string logHeader = "PluginsMgr." + this->PluginsMgr_.Name_;
      this->ConfigFileBinder_.Write(&logHeader, BufferTo<std::string>(rbuf.MoveOut()));
   }
   std::string BindConfigFile(StrView cfgfn) {
      std::string res;
      this->TaskQu_.InplaceOrWait(AQueueTaskKind::Set, [this, cfgfn, &res]() {
         std::string       logHeader = "PluginsMgr." + this->PluginsMgr_.Name_;
         ConfigFileBinder& cfgbinder = this->ConfigFileBinder_;
         res = cfgbinder.OpenRead(&logHeader, cfgfn.ToString());
         if (res.empty()) {
            res = this->ParseConfigStr(&cfgbinder.GetConfigStr());
            if (0);// TODO: 載入成功, 備份設定檔.
         }
      });
      return res;
   }
   std::string ParseConfigStr(StrView cfgstr) {
      struct ToContainer : public GridViewToContainer {
         PluginsTree* PluginsTree_;
         ToContainer(PluginsTree* pluginsTree) : PluginsTree_{pluginsTree} {
         }
         void OnNewRow(StrView keyText, StrView ln) override {
            PluginsRec& rec = **this->PluginsTree_->PluginsRecs_.insert(PluginsRecSP{new PluginsRec(this->PluginsTree_, keyText)}).first;
            this->FillRaw(SimpleRawWr{rec}, ln);
         }
      };
      ToContainer{this}.ParseConfigStr(this->LayoutSP_->GetTab(0)->Fields_, cfgstr);
      for (auto& irec : this->PluginsRecs_) {
         PluginsRec& rec = *irec;
         if (rec.Enabled_ == EnabledYN::Yes)
            rec.InThr_StartPlugins();
      }
      return std::string{};
   }
};
//--------------------------------------------------------------------------//
void PluginsMgr::PluginsTree::TaskInvoker::MakeCallForWork() {
   PluginsTreeSP pthis{&ContainerOf(TaskQu::StaticCast(*this), &PluginsTree::TaskQu_)};
   GetDefaultThreadPool().EmplaceMessage([pthis]() {
      pthis->TaskQu_.TakeCall();
   });
}
//--------------------------------------------------------------------------//
PluginsMgr::PluginsRec::PluginsRec(PluginsTreeSP owner, StrView id)
   : PluginsHolder{std::move(owner->Root_)}
   , Owner_{std::move(owner)}
   , Id_{id} {
}
void PluginsMgr::PluginsRec::SetPluginStImpl(std::string stmsg) {
   // 避免走到 TaskQu_ thread 時, this 已經死亡, 所以使用 pthis 保留 this.
   PluginsRecSP pthis{this};
   this->Owner_->TaskQu_.AddTask([pthis, stmsg]() {
      pthis->Status_.assign(&stmsg);
   });
}
void PluginsMgr::PluginsRec::InThr_StopPlugins() {
   assert(this->Owner_->TaskQu_.InTakingCallThread());
   if (this->GetPluginsDesc())
      this->SetPluginsSt(LogLevel::Info, "Plugins stop");
   this->StopPlugins();
}
void PluginsMgr::PluginsRec::InThr_StartPlugins() {
   std::string res = this->LoadPlugins(ToStrView(this->FileName_), ToStrView(this->EntryName_));
   if (!res.empty())
      this->SetPluginsSt(LogLevel::Error, res);
   else if (auto desc = this->GetPluginsDesc()) {
      this->Description_.assign(StrView_cstr(desc->Description_));
      this->SetPluginsSt(LogLevel::Info, "Plugins running");
      this->StartPlugins(ToStrView(this->Args_));
   }
}
void PluginsMgr::PluginsRec::InThr_OnChanged() {
   assert(this->Owner_->TaskQu_.InTakingCallThread());
   this->Owner_->InThr_OnConfigChanged();
   if (this->Enabled_ == EnabledYN::Yes)
      this->InThr_StartPlugins();
   else
      this->InThr_StopPlugins();
}
//--------------------------------------------------------------------------//
TreeSP PluginsMgr::MakePluginsSapling(PluginsMgr& pluginsMgr, MaTreeSP&& root) {
   return TreeSP{new PluginsTree{pluginsMgr, std::move(root)}};
}
std::string PluginsMgr::BindConfigFile(StrView cfgfn) {
   std::string res = static_cast<PluginsTree*>(this->Sapling_.get())->BindConfigFile(cfgfn);
   this->SetTitle(cfgfn.ToString());
   this->SetDescription(res);
   return res;
}
fon9_WARN_POP;

} } // namespaces
