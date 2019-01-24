/// \file fon9/framework/IoManagerTree.cpp
/// \author fonwinz@gmail.com
#include "fon9/framework/IoManagerTree.hpp"
#include "fon9/seed/TabTreeOp.hpp"
#include "fon9/seed/ConfigGridView.hpp"

namespace fon9 {

seed::LayoutSP IoManagerTree::MakeLayoutImpl() {
   seed::LayoutSP saplingLayout = IoManager::GetAcceptedClientLayout();
   seed::Fields   fields;
   fields.Add(fon9_MakeField(Named{"Session"},   DeviceItem, Config_.SessionName_));
   fields.Add(fon9_MakeField(Named{"SessionSt"}, DeviceItem, SessionSt_));
   fields.Add(fon9_MakeField(Named{"Device"},    DeviceItem, Config_.DeviceName_));
   fields.Add(fon9_MakeField(Named{"DeviceSt"},  DeviceItem, DeviceSt_));
   fields.Add(fon9_MakeField(Named{"OpenArgs"},  DeviceItem, OpenArgs_));
   seed::TabSP tabSt{new seed::Tab(Named{"Status"}, std::move(fields), saplingLayout)};

   fields.Add(fon9_MakeField(Named{"Enabled"},     DeviceItem, Config_.Enabled_));
   fields.Add(fon9_MakeField(Named{"Sch"},         DeviceItem, Config_.SchArgs_));
   fields.Add(fon9_MakeField(Named{"Session"},     DeviceItem, Config_.SessionName_));
   fields.Add(fon9_MakeField(Named{"SessionArgs"}, DeviceItem, Config_.SessionArgs_));
   fields.Add(fon9_MakeField(Named{"Device"},      DeviceItem, Config_.DeviceName_));
   fields.Add(fon9_MakeField(Named{"DeviceArgs"},  DeviceItem, Config_.DeviceArgs_));
   seed::TabSP tabConfig{new seed::Tab(Named{"Config"}, std::move(fields), saplingLayout, seed::TabFlag::NeedsApply)};

   #define kTabStatusIndex  0
   #define kTabConfigIndex  1
   return new seed::LayoutN(fon9_MakeField(Named{"Id"}, DeviceItem, Id_),
                            std::move(tabSt),
                            std::move(tabConfig));
}
seed::LayoutSP IoManagerTree::GetLayout() {
   static seed::LayoutSP Layout_{MakeLayoutImpl()};
   return Layout_;
}
//--------------------------------------------------------------------------//
fon9_WARN_DISABLE_PADDING;
fon9_MSC_WARN_DISABLE_NO_PUSH(4265 /* class has virtual functions, but destructor is not virtual. */
                              4355 /* 'this' : used in base member initializer list*/);
IoManagerTree::IoManagerTree(const IoManagerArgs& args)
   : baseTree{IoManagerTree::GetLayout()}
   , IoManager(args)
   , TabTreeOp_{new seed::TabTreeOp(*this->LayoutSP_->GetTab(kTabConfigIndex))} {
   this->SubConnDev_ = this->DeviceFactoryPark_->Subscribe([this](DeviceFactory*, seed::ParkTree::EventType) {
      this->OnFactoryParkChanged();
   });
   this->SubConnSes_ = this->SessionFactoryPark_->Subscribe([this](SessionFactory*, seed::ParkTree::EventType) {
      this->OnFactoryParkChanged();
   });
   args.Result_.clear();
   if (args.CfgFileName_.empty())
      return;
   std::string logHeader = "IoManager." + args.Name_;
   args.Result_ = this->ConfigFileBinder_.OpenRead(&logHeader, args.CfgFileName_);
   if (!args.Result_.empty())
      return;
   struct ToContainer : public seed::GridViewToContainer {
      DeviceMapImpl* Container_;
      ToContainer(DeviceMapImpl* container) : Container_{container} {
      }
      void OnNewRow(StrView keyText, StrView ln) override {
         DeviceMapImpl::value_type& item = *this->Container_->insert(new DeviceItem{keyText}).first;
         this->FillRaw(seed::SimpleRawWr{*item}, ln);
         item->Sch_.Parse(ToStrView(item->Config_.SchArgs_));
      }
   };
   DeviceMap::Locker curmap{this->DeviceMap_};
   ToContainer       toContainer{curmap.get()};
   toContainer.ParseConfigStr(this->LayoutSP_->GetTab(kTabConfigIndex)->Fields_,
                              &this->ConfigFileBinder_.GetConfigStr());
   this->StartTimerForOpen();
}
IoManagerTree::~IoManagerTree() {
   this->DeviceFactoryPark_->Unsubscribe(this->SubConnDev_);
   this->SessionFactoryPark_->Unsubscribe(this->SubConnSes_);
   this->Timer_.StopAndWait();
}
unsigned IoManagerTree::IoManagerAddRef() {
   return intrusive_ptr_add_ref(static_cast<baseTree*>(this));
}
unsigned IoManagerTree::IoManagerRelease() {
   return intrusive_ptr_release(static_cast<baseTree*>(this));
}
void IoManagerTree::OnFactoryParkChanged() {
   this->Timer_.RunAfter(TimeInterval_Second(1));
}
void IoManagerTree::StartTimerForOpen() {
   this->Timer_.RunAfter(TimeInterval_Second(1));
   this->TimerFor_ = TimerFor::Open;
}
void IoManagerTree::EmitOnTimer(TimerEntry* timer, TimeStamp now) {
   IoManagerTree&    rthis = ContainerOf(*static_cast<Timer*>(timer), &IoManagerTree::Timer_);
   TimeStamp         nextCheckTime;
   DeviceMap::Locker curmap{rthis.DeviceMap_};
   for (auto& item : *curmap) {
      bool isChanged = false;
      switch (rthis.TimerFor_) {
      case TimerFor::Open:
         if (item->Config_.Enabled_ != EnabledYN::Yes) {
            isChanged = item->DisposeDevice(now, "Disabled");
            break;
         }
         // 不用 break; 檢查 item 是否在時間排程內.
      case TimerFor::CheckSch:
      default:
         if (item->Config_.Enabled_ != EnabledYN::Yes)
            break;
         // 必須是 Enabled 才需檢查排程.
         auto res = item->Sch_.Check(now);
         if (nextCheckTime == TimeStamp{} || nextCheckTime > res.NextCheckTime_)
            nextCheckTime = res.NextCheckTime_;
         if (item->SchSt_ == res.SchSt_) {
            if (res.SchSt_ != SchSt::In && rthis.TimerFor_ == TimerFor::Open)
               goto __SET_ST_OUT_SCH;
            break;
         }
         item->SchSt_ = res.SchSt_;
         if (res.SchSt_ == SchSt::In) {
            rthis.CheckOpenDevice(*item);
            if (!item->Device_) {
               // 開啟失敗: Factory 不正確? Factory.CreateDevice() 失敗?
               // 設為 SchSt::Unknown, 讓下次檢查 sch 時, 再 CheckOpenDevice() 一次.
               item->SchSt_ = SchSt::Unknown;
            }
            isChanged = true;
         }
         else {
         __SET_ST_OUT_SCH:
            isChanged = item->DisposeDevice(now, "Out of schedule");
         }
         break;
      }
      if (isChanged)
         rthis.NotifyChanged(*item);
   }
   // 必須在 locked 狀態下啟動 timer,
   // 因為如果此時其他 thread 設定 TimerFor::Open; timer->RunAfter();
   // 然後才執行下面這行, 那時間就錯了!
   if (nextCheckTime != TimeStamp{})
      timer->RunAt(nextCheckTime);
}
void IoManagerTree::NotifyChanged(DeviceItem& item) {
   seed::SeedSubj_Notify(this->StatusSubj_, *this, *this->LayoutSP_->GetTab(kTabStatusIndex), ToStrView(item.Id_), item);
}
void IoManagerTree::NotifyChanged(DeviceRun& item) {
   if (item.IsDeviceItem())
      this->NotifyChanged(*static_cast<DeviceItem*>(&item));
}
void IoManagerTree::OnParentSeedClear() {
   {  // dispose all devices.
      DeviceMap::Locker map{this->DeviceMap_};
      for (auto& i : *map) {
         if (io::Device* dev = i->Device_.get())
            dev->AsyncDispose("Parent clear");
      }
   }  // unlock map;
   SeedSubj_ParentSeedClear(this->StatusSubj_, *this);
}
//--------------------------------------------------------------------------//
struct IoManagerTree::PodOp : public seed::PodOpLockerNoWrite<PodOp, DeviceMap::Locker> {
   fon9_NON_COPY_NON_MOVE(PodOp);
   using base = seed::PodOpLockerNoWrite<PodOp, DeviceMap::Locker>;
   DeviceItemSP Seed_;
   PodOp(DeviceItem& seed, Tree& sender, seed::OpResult res, DeviceMap::Locker& locker)
      : base{*this, sender, res, ToStrView(seed.Id_), locker}
      , Seed_{&seed} {
   }
   DeviceItem& GetSeedRW(seed::Tab&) {
      return *this->Seed_;
   }
   seed::TreeSP HandleGetSapling(seed::Tab&) {
      return this->Seed_->Sapling_;
   }
   void HandleSeedCommand(DeviceMap::Locker& locker, SeedOpResult& res, StrView cmdln, seed::FnCommandResultHandler&& resHandler) {
      res.OpResult_ = seed::OpResult::no_error;
      io::DeviceSP dev = this->Seed_->Device_;
      if (dev) {
         locker.unlock();
         std::string msg = dev->DeviceCommand(cmdln);
         resHandler(res, &msg);
         return;
      }
      StrView cmd = StrFetchTrim(cmdln, &isspace);
      if (cmd == "open") {
         if (static_cast<IoManagerTree*>(this->Sender_)->CreateDevice(*this->Seed_) == DeviceOpenResult::NewDeviceCreated) {
            if (cmdln.empty())
               cmdln = ToStrView(this->Seed_->Config_.DeviceArgs_);
            AssignStStr(this->Seed_->DeviceSt_, UtcNow(), "Async opening by command.");
            this->Seed_->Device_->AsyncOpen(cmdln.ToString());
         }
         auto msg = this->Seed_->DeviceSt_;
         locker.unlock();
         resHandler(res, ToStrView(msg));
         return;
      }
      locker.unlock();
      if (cmd == "?") {
         res.OpResult_ = seed::OpResult::no_error;
         resHandler(res,
                    "open" fon9_kCSTR_CELLSPL "Open device" fon9_kCSTR_CELLSPL "[ConfigString] or Default config.");
      }
      else {
         res.OpResult_ = seed::OpResult::not_supported_cmd;
         resHandler(res, cmdln);
      }
   }
};
struct IoManagerTree::TreeOp : public seed::TreeOp {
   fon9_NON_COPY_NON_MOVE(TreeOp);
   using base = seed::TreeOp;
   TreeOp(Tree& tree) : base(tree) {
   }
   static void MakePolicyRecordView(DeviceMapImpl::iterator ivalue, seed::Tab* tab, RevBuffer& rbuf) {
      if (tab)
         FieldsCellRevPrint(tab->Fields_, seed::SimpleRawRd{**ivalue}, rbuf, seed::GridViewResult::kCellSplitter);
      RevPrint(rbuf, (*ivalue)->Id_);
   }
   void GridView(const seed::GridViewRequest& req, seed::FnGridViewOp fnCallback) override {
      seed::GridViewResult res{this->Tree_, req.Tab_};
      {
         DeviceMap::Locker  map{static_cast<IoManagerTree*>(&this->Tree_)->DeviceMap_};
         seed::MakeGridView(*map, seed::GetIteratorForGv(*map, req.OrigKey_),
                            req, res, &MakePolicyRecordView);
      } // unlock.
      fnCallback(res);
   }
   void Get(StrView strKeyText, seed::FnPodOp fnCallback) override {
      {
         DeviceMap::Locker  map{static_cast<IoManagerTree*>(&this->Tree_)->DeviceMap_};
         auto               ifind = seed::GetIteratorForPod(*map, strKeyText);
         if (ifind != map->end()) {
            PodOp op(**ifind, *static_cast<IoManagerTree*>(&this->Tree_), seed::OpResult::no_error, map);
            fnCallback(op, &op);
            return;
         }
      } // unlock.
      fnCallback(seed::PodOpResult{this->Tree_, seed::OpResult::not_found_key, strKeyText}, nullptr);
   }
   void GridApplySubmit(const seed::GridApplySubmitRequest& req, seed::FnCommandResultHandler fnCallback) override {
      if (!req.Tab_ || req.Tab_->GetIndex() != kTabConfigIndex)
         return base::GridApplySubmit(req, std::move(fnCallback));

      StrView              resmsg;
      seed::SeedOpResult   res{this->Tree_, seed::OpResult::no_error, seed::kTabTree_KeyApply, req.Tab_};
      seed::GridViewResult gvres{this->Tree_, req.Tab_};
      DeviceMapImpl        oldmap;
      {  // lock curmap.
         DeviceMap::Locker curmap{static_cast<IoManagerTree*>(&this->Tree_)->DeviceMap_};
         if (req.BeforeGrid_.begin() != nullptr) {
            seed::MakeGridView(*curmap, curmap->begin(), seed::GridViewRequestFull{*req.Tab_}, gvres, &MakePolicyRecordView);
            if (req.BeforeGrid_ != ToStrView(gvres.GridView_)) {
               res.OpResult_ = seed::OpResult::bad_apply_submit;
               resmsg = "Orig tree changed";
               goto __UNLOCK_AND_CALLBACK;
            }
         }
         IoManagerTree::Apply(req.Tab_->Fields_, req.EditingGrid_, *curmap, oldmap);
      }  // unlock curmap.
      // After grid view apply. oldmap = removed items.
      if (!oldmap.empty()) {
         TimeStamp now = UtcNow();
         for (auto& i : oldmap) {
            i->DisposeDevice(now, "Removed from ApplySubmit");
            // StatusSubj_ 使用 [整表] 通知: seed::SeedSubj_TableChanged();
            // 所以這裡就不用樹發 PodRemoved() 事件了.
            // seed::SeedSubj_NotifyPodRemoved(static_cast<IoManagerTree*>(&this->Tree_)->StatusSubj_, this->Tree_, ToStrView(i->Id_));
         }
      }
      if (static_cast<IoManagerTree*>(&this->Tree_)->ConfigFileBinder_.HasBinding()) {
         RevBufferList rbuf{128};
         RevPutChar(rbuf, *fon9_kCSTR_ROWSPL);
         seed::RevPrintConfigFieldNames(rbuf, req.Tab_->Fields_, &this->Tree_.LayoutSP_->KeyField_->Name_);
         static_cast<IoManagerTree*>(&this->Tree_)->ConfigFileBinder_.WriteConfig(
            ToStrView("IoManager." + static_cast<IoManagerTree*>(&this->Tree_)->Name_),
            BufferTo<std::string>(rbuf.MoveOut()) + req.EditingGrid_.ToString());
      }
      static_cast<IoManagerTree*>(&this->Tree_)->StartTimerForOpen();

   __UNLOCK_AND_CALLBACK:
      fnCallback(res, resmsg);
      if (res.OpResult_ == seed::OpResult::no_error)
         seed::SeedSubj_TableChanged(static_cast<IoManagerTree*>(&this->Tree_)->StatusSubj_,
                                     this->Tree_, *this->Tree_.LayoutSP_->GetTab(kTabStatusIndex));
   }
   seed::OpResult Subscribe(SubConn* pSubConn, seed::Tab& tab, seed::SeedSubr subr) override {
      if (&tab != this->Tree_.LayoutSP_->GetTab(kTabStatusIndex))
         return this->SubscribeUnsupported(pSubConn);
      *pSubConn = static_cast<IoManagerTree*>(&this->Tree_)->StatusSubj_.Subscribe(subr);
      return seed::OpResult::no_error;
   }
   seed::OpResult Unsubscribe(SubConn pSubConn) override {
      return static_cast<IoManagerTree*>(&this->Tree_)->StatusSubj_.Unsubscribe(pSubConn)
         ? seed::OpResult::no_error : seed::OpResult::not_supported_subscribe;
   }
};
fon9_WARN_POP;

void IoManagerTree::OnTreeOp(seed::FnTreeOp fnCallback) {
   TreeOp op{*this};
   fnCallback(seed::TreeOpResult{this, seed::OpResult::no_error}, &op);
}
void IoManagerTree::OnTabTreeOp(seed::FnTreeOp fnCallback) {
   TreeOp op{*this};
   static_cast<seed::TabTreeOp*>(this->TabTreeOp_.get())->HandleTabTreeOp(op, std::move(fnCallback));
}
//--------------------------------------------------------------------------//
void IoManagerTree::Apply(const seed::Fields& flds, StrView src, DeviceMapImpl& curmap, DeviceMapImpl& oldmap) {
   struct ToContainer : public seed::GridViewToContainer {
      DeviceMapImpl* CurContainer_;
      DeviceMapImpl* OldContainer_;
      IoConfigItem   OldCfg_;
      TimeStamp      Now_{UtcNow()};
      void OnNewRow(StrView keyText, StrView ln) override {
         auto        ifind = this->OldContainer_->find(keyText);
         DeviceItem* item;
         bool        isNewItem = (ifind == this->OldContainer_->end());
         if (isNewItem)
            item = this->CurContainer_->insert(new DeviceItem{keyText}).first->get();
         else {
            item = this->CurContainer_->insert(std::move(*ifind)).first->get();
            this->OldContainer_->erase(ifind);
            this->OldCfg_ = item->Config_;
         }
         this->FillRaw(seed::SimpleRawWr{*item}, ln);
         if (!isNewItem) {
            if (item->Config_ == this->OldCfg_)
               return;
            item->DisposeDevice(this->Now_, "Config changed");
            item->SchSt_ = SchSt::Unknown;
         }
         item->Sch_.Parse(ToStrView(item->Config_.SchArgs_));
      }
   };
   ToContainer toContainer;
   toContainer.CurContainer_ = &curmap;
   toContainer.OldContainer_ = &oldmap;
   curmap.swap(oldmap);
   flds.GetAll(toContainer.Fields_);
   toContainer.ParseConfigStr(src);
}

} // namespaces
