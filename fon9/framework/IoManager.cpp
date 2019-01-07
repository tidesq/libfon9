/// \file fon9/framework/IoManager.cpp
/// \author fonwinz@gmail.com
#include "fon9/framework/IoManager.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/seed/TabTreeOp.hpp"
#include "fon9/seed/ConfigGridView.hpp"
#include "fon9/Log.hpp"

namespace fon9 {

static void AssignStStr(CharVector& dst, TimeStamp now, StrView stmsg) {
   size_t sz = kDateTimeStrWidth + stmsg.size() + 1;
   // 底下 -+sizeof(NumOutBuf) 是因為 RevPrint() 需要分配較多的保留大小.
   // 但因我確定實際大小不會超過, 所以自行調整.
   RevBufferFixedMem rbuf{static_cast<char*>(dst.alloc(sz)) - sizeof(NumOutBuf), sz + sizeof(NumOutBuf)};
   RevPrint(rbuf, now, '|', stmsg);
}

//--------------------------------------------------------------------------//

IoManager::IoManager(Tree& ownerTree, const IoManagerArgs& args)
   : Name_{args.Name_}
   , SessionFactoryPark_{args.SessionFactoryPark_ ? args.SessionFactoryPark_ : new SessionFactoryPark{"SessionFactoryPark"}}
   , DeviceFactoryPark_{args.DeviceFactoryPark_ ? args.DeviceFactoryPark_ : new DeviceFactoryPark{"DeviceFactoryPark"}}
   , OwnerTree_(&ownerTree)
   , IoServiceCfgstr_{args.IoServiceCfgstr_} {
   if (args.IoServiceSrc_) {
   #ifdef __fon9_io_win_IocpService_hpp__
      this->IocpService_ = args.IoServiceSrc_->GetIocpService();
   #endif
   #ifdef __fon9_io_FdrService_hpp__
      this->FdrService_ = args.IoServiceSrc_->GetFdrService();
   #endif
   }
}

void IoManager::ParseIoServiceArgs(io::IoServiceArgs& iosvArgs) const {
   RevBufferList rbuf{128};
   RevPutChar(rbuf, '\n');
   if (!ParseConfig(iosvArgs, &this->IoServiceCfgstr_, rbuf)) {
      // fon9_LOG_ERROR:
      RevPrint(rbuf, "IoManager.", this->Name_, ".CreateIoService|info=use default config");
      LogWrite(LogLevel::Error, std::move(rbuf));
   }
}

void IoManager::RaiseMakeIoServiceFatal(Result2 err) const {
   std::string errmsg = RevPrintTo<std::string>("IoManager.", this->Name_, ".MakeIoService|err=", err);
   fon9_LOG_FATAL(errmsg);
   Raise<std::runtime_error>(std::move(errmsg));
}

template <class IoService>
intrusive_ptr<IoService> IoManager::MakeIoService() const{
   io::IoServiceArgs iosvArgs;
   this->ParseIoServiceArgs(iosvArgs);
   typename IoService::MakeResult err;
   auto iosv = IoService::MakeService(iosvArgs, this->Name_, err);
   if (!iosv)
      this->RaiseMakeIoServiceFatal(err);
   return iosv;
}

#ifdef __fon9_io_win_IocpService_hpp__
io::IocpServiceSP IoManager::GetIocpService() {
   if (!this->IocpService_)
      this->IocpService_ = this->MakeIoService<io::IocpService>();
   return this->IocpService_;
}
#endif

#ifdef __fon9_io_FdrService_hpp__
io::FdrServiceSP IoManager::GetFdrService() {
   if (!this->FdrService_) {
      io::IoServiceArgs iosvArgs;
      this->ParseIoServiceArgs(iosvArgs);

      Result2  err;
      this->FdrService_ = MakeDefaultFdrService(iosvArgs, this->Name_, err);
      if (!this->FdrService_)
         this->RaiseMakeIoServiceFatal(err);
   }
   return this->FdrService_;
}
#endif

bool IoManager::AddConfig(StrView id, const IoConfigItem& cfg) {
   DeviceItemSP      item{new DeviceItem{id, cfg}};
   DeviceMap::Locker curmap{this->DeviceMap_};
   if (curmap->insert(item).second)
      return this->CheckOpenDevice(*item) == DeviceOpenResult::NewDeviceCreated;
   return false;
}
IoManager::DeviceOpenResult IoManager::CreateDevice(DeviceItem& item) {
   if (item.Device_)
      return DeviceOpenResult::AlreadyExists;
   item.SessionSt_.clear();
   item.DeviceSt_.clear();
   auto sesFactory = this->SessionFactoryPark_->Get(ToStrView(item.Config_.SessionName_));
   if (!sesFactory) {
      AssignStStr(item.SessionSt_, UtcNow(), "Session factory not found.");
      return DeviceOpenResult::SessionFactoryNotFound;
   }
   if (auto devFactory = this->DeviceFactoryPark_->Get(ToStrView(item.Config_.DeviceName_))) {
      std::string errReason;
      if (!(item.Device_ = devFactory->CreateDevice(this, *sesFactory, item.Config_, errReason))) {
         errReason = "IoManager.CreateDevice|err=" + errReason;
         AssignStStr(item.DeviceSt_, UtcNow(), &errReason);
         return DeviceOpenResult::DeviceCreateError;
      }
      item.Device_->SetManagerBookmark(reinterpret_cast<io::Bookmark>(&item));
      item.Device_->Initialize();
      return DeviceOpenResult::NewDeviceCreated;
   }
   AssignStStr(item.DeviceSt_, UtcNow(), "Device factory not found");
   return DeviceOpenResult::DeviceFactoryNotFound;
}
IoManager::DeviceOpenResult IoManager::CheckOpenDevice(DeviceItem& item) {
   if (item.Config_.Enabled_ != EnabledYN::Yes)
      return DeviceOpenResult::Disabled;
   DeviceOpenResult res;
   if (item.Device_) {
      if (item.OpenArgs_ == item.Config_.DeviceArgs_)
         return DeviceOpenResult::AlreadyExists;
      res = DeviceOpenResult::AlreadyExists;
   }
   else {
      res = this->CreateDevice(item);
      if (!item.Device_)
         return res;
   }
   if (item.Config_.DeviceArgs_.empty())
      // 沒提供 DeviceArgs_, 則由使用者 or Session 手動開啟.
      AssignStStr(item.DeviceSt_, UtcNow(), "Waiting for manual open");
   else {
      AssignStStr(item.DeviceSt_, UtcNow(), "Async opening");
      item.Device_->AsyncOpen(item.Config_.DeviceArgs_.ToString());
   }
   return res;
}
void IoManager::Apply(const seed::Fields& flds, StrView src, DeviceMapImpl& curmap, DeviceMapImpl& oldmap) {
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

//--------------------------------------------------------------------------//

IoManager::DeviceRun* IoManager::FromManagerBookmark(io::Device& dev) {
   DeviceRun* item = reinterpret_cast<DeviceRun*>(dev.GetManagerBookmark());
   if (item && item->Device_.get() == &dev)
      return item;
   return nullptr;
}
void IoManager::OnDevice_Initialized(io::Device&) {
   // accepted client 在 OnDevice_Accepted() 時設定 SetManagerBookmark();
   // 一般 device 在 Device::Initialize() 之後設定 SetManagerBookmark();
   // 所以這裡不用做任何事情.
}
void IoManager::OnDevice_Destructing(io::Device& dev) {
   if (DeviceRun* item = reinterpret_cast<DeviceRun*>(dev.GetManagerBookmark())) {
      // 只有 AcceptedClient 在 OnDevice_Destructing() 時還會保留 ManagerBookmark.
      // 所以此時的 item 可以安全的刪除.
      delete item;
      // TODO: 如果 dev 是 AcceptedClient, 則更新 server st (accepted client 的剩餘數量)?
   }
   fon9_LOG_TRACE("IoManager.", this->Name_, ".OnDevice_Destructing|dev=", ToPtr(&dev));
}
void IoManager::OnDevice_Accepted(io::DeviceServer& server, io::DeviceAcceptedClient& client) {
   DeviceRun* cliItem = new DeviceRun; // 在 OnDevice_Destructing() 時刪除.
   cliItem->Device_.reset(&client);
   cliItem->AcceptedClientSeq_ = client.GetAcceptedClientSeq();
   client.SetManagerBookmark(reinterpret_cast<io::Bookmark>(cliItem));

   DeviceMap::Locker map{this->DeviceMap_};
   if (auto serItem = this->FromManagerBookmark(server)) {
      this->MakeAcceptedClientTree(*serItem, client.Owner_);
      cliItem->Sapling_ = serItem->Sapling_;
   }
}
void IoManager::OnDevice_StateChanged(io::Device& dev, const io::StateChangedArgs& e) {
   this->UpdateDeviceState(dev, e.After_);
}
void IoManager::OnDevice_StateUpdated(io::Device& dev, const io::StateUpdatedArgs& e) {
   this->UpdateDeviceState(dev, e);
}
void IoManager::UpdateDeviceState(io::Device& dev, const io::StateUpdatedArgs& e) {
   if (io::DeviceAcceptedClient* cli = dynamic_cast<io::DeviceAcceptedClient*>(&dev)) {
      auto clis = cli->Owner_->Lock();
      this->UpdateDeviceStateLocked(dev, e);
   }
   else {
      DeviceMap::Locker map{this->DeviceMap_};
      this->UpdateDeviceStateLocked(dev, e);
   }
}
void IoManager::UpdateDeviceStateLocked(io::Device& dev, const io::StateUpdatedArgs& e) {
   RevBufferList rbuf{128};
   RevPrint(rbuf,
            "|st=", GetStateStr(e.State_),
            "|id={", e.DeviceId_, "}"
            "|info=", e.Info_, '\n');
   if (auto item = this->FromManagerBookmark(dev)) {
      const BufferNode* bnode = rbuf.cfront();
      char* pmsg = static_cast<char*>(item->DeviceSt_.alloc(kDateTimeStrWidth + CalcDataSize(bnode)));
      ToStrRev(pmsg += kDateTimeStrWidth, UtcNow());
      pmsg = static_cast<char*>(CopyNodeList(pmsg, bnode));
      *(pmsg - 1) = 0; // for back '\n' => '\0';
      item->DeviceSt_.resize(item->DeviceSt_.size() - 1); // remove back '\n'
      if (e.State_ == io::State::Opening)
         item->OpenArgs_.assign(e.Info_);
      else if (e.State_ == io::State::Disposing) {
         item->Device_.reset();
         item->Sapling_.reset();
         if (item->AcceptedClientSeq_ == 0)
            dev.SetManagerBookmark(0);
         // item->AcceptedClientSeq_ != 0, accepted client:
         // 保留 Bookmark, 等 OnDevice_Destructing() 事件時, 將 item 刪除.
      }
      if (this->OwnerTree_)
         this->OwnerTree_->NotifyChanged(*item);
   }
   static const LogLevel lvs[] {
      LogLevel::Trace, // Initializing,
      LogLevel::Trace, // Initialized,
      LogLevel::Warn,  // ConfigError,
      LogLevel::Trace, // Opening,
      LogLevel::Info,  // WaitingLinkIn,
      LogLevel::Trace, // Linking,
      LogLevel::Warn,  // LinkError,
      LogLevel::Info,  // LinkReady,
      LogLevel::Warn,  // LinkBroken,
      LogLevel::Info,  // Listening,
      LogLevel::Warn,  // ListenBroken,
      LogLevel::Trace, // Lingering,
      LogLevel::Trace, // Closing,
      LogLevel::Info,  // Closed,
      LogLevel::Info,  // Disposing,
      LogLevel::Info,  // Destructing,
   };
   const LogLevel lv = static_cast<unsigned>(e.State_) < numofele(lvs)
                     ? lvs[static_cast<unsigned>(e.State_)] : LogLevel::Fatal;
   if (fon9_UNLIKELY(lv >= LogLevel_)) {
      RevPrint(rbuf, "IoManager.", this->Name_, ".DeviceState|dev=", ToPtr(&dev));
      LogWrite(lv, std::move(rbuf));
   }
}
void IoManager::OnSession_StateUpdated(io::Device& dev, StrView stmsg, LogLevel lv) {
   if (io::DeviceAcceptedClient* cli = dynamic_cast<io::DeviceAcceptedClient*>(&dev)) {
      auto clis = cli->Owner_->Lock();
      this->UpdateSessionStateLocked(dev, stmsg, lv);
   }
   else {
      DeviceMap::Locker map{this->DeviceMap_};
      this->UpdateSessionStateLocked(dev, stmsg, lv);
   }
}
void IoManager::UpdateSessionStateLocked(io::Device& dev, StrView stmsg, LogLevel lv) {
   if (auto item = this->FromManagerBookmark(dev)) {
      AssignStStr(item->SessionSt_, UtcNow(), stmsg);
      if (this->OwnerTree_)
         this->OwnerTree_->NotifyChanged(*item);
   }
   fon9_LOG(lv, "IoManager.", this->Name_, ".SessionState|dev=", ToPtr(&dev), "|st=", stmsg);
}

//--------------------------------------------------------------------------//

seed::LayoutSP IoManager::MakeAcceptedClientLayout() {
   struct Aux {
      static seed::LayoutSP MakeLayout() {
         seed::Fields fields;
         fields.Add(fon9_MakeField(Named{"SessionSt"}, DeviceRun, SessionSt_));
         fields.Add(fon9_MakeField(Named{"DeviceSt"},  DeviceRun, DeviceSt_));
         fields.Add(fon9_MakeField(Named{"OpenArgs"},  DeviceRun, OpenArgs_));
         seed::TabSP tabSt{new seed::Tab(Named{"Status"}, std::move(fields), seed::TabFlag::HasSameCommandsSet)};
         return new seed::Layout1(seed::MakeField(Named{"Id"}, 0, *static_cast<const io::DeviceAcceptedClientSeq*>(nullptr)),
                                  std::move(tabSt));
      }
   };
   static seed::LayoutSP layout{Aux::MakeLayout()};
   return layout;
}
seed::LayoutSP IoManager::Tree::MakeLayout() {
   struct Aux {
      static seed::LayoutSP MakeLayoutImpl() {
         seed::LayoutSP saplingLayout = IoManager::MakeAcceptedClientLayout();
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
   };
   static seed::LayoutSP Layout_{Aux::MakeLayoutImpl()};
   return Layout_;
}
void IoManager::Tree::OnParentSeedClear() {
   {  // dispose all devices.
      DeviceMap::Locker map{this->IoManager_->DeviceMap_};
      this->IoManager_->OwnerTree_ = nullptr;
      for (auto& i : *map) {
         if (io::Device* dev = i->Device_.get())
            dev->AsyncDispose("Parent clear");
      }
   }  // unlock map;
   SeedSubj_ParentSeedClear(this->StatusSubj_, *this);
}

//--------------------------------------------------------------------------//

fon9_WARN_DISABLE_PADDING;
fon9_MSC_WARN_DISABLE_NO_PUSH(4265 /* class has virtual functions, but destructor is not virtual. */
                              4355 /* 'this' : used in base member initializer list*/);
IoManager::Tree::Tree(IoManagerArgs& args)
   : base{MakeLayout()}
   , TabTreeOp_{new seed::TabTreeOp(*this->LayoutSP_->GetTab(kTabConfigIndex))}
   , IoManager_{new IoManager{*this, args}} {
   this->SubConnDev_ = IoManager_->DeviceFactoryPark_->Subscribe([this](DeviceFactory*, seed::ParkTree::EventType) {
      this->OnFactoryParkChanged();
   });
   this->SubConnSes_ = IoManager_->SessionFactoryPark_->Subscribe([this](SessionFactory*, seed::ParkTree::EventType) {
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
   DeviceMap::Locker curmap{this->IoManager_->DeviceMap_};
   ToContainer       toContainer{curmap.get()};
   toContainer.ParseConfigStr(this->LayoutSP_->GetTab(kTabConfigIndex)->Fields_,
                              &this->ConfigFileBinder_.GetConfigStr());
   this->StartTimerForOpen();
}
IoManager::Tree::~Tree() {
   this->IoManager_->DeviceFactoryPark_->Unsubscribe(this->SubConnDev_);
   this->IoManager_->SessionFactoryPark_->Unsubscribe(this->SubConnSes_);
   this->Timer_.StopAndWait();
}
void IoManager::Tree::OnFactoryParkChanged() {
   this->Timer_.RunAfter(TimeInterval_Second(1));
}
void IoManager::Tree::StartTimerForOpen() {
   this->Timer_.RunAfter(TimeInterval_Second(1));
   this->TimerFor_ = TimerFor::Open;
}
void IoManager::Tree::EmitOnTimer(TimerEntry* timer, TimeStamp now) {
   Tree&             rthis = ContainerOf(*static_cast<Timer*>(timer), &Tree::Timer_);
   TimeStamp         nextCheckTime;
   DeviceMap::Locker curmap{rthis.IoManager_->DeviceMap_};
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
            rthis.IoManager_->CheckOpenDevice(*item);
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
void IoManager::Tree::NotifyChanged(DeviceItem& item) {
   seed::SeedSubj_Notify(this->StatusSubj_, *this, *this->LayoutSP_->GetTab(kTabStatusIndex), ToStrView(item.Id_), item);
}
void IoManager::Tree::NotifyChanged(DeviceRun& item) {
   if (item.IsDeviceItem())
      this->NotifyChanged(*static_cast<DeviceItem*>(&item));
}

struct IoManager::Tree::PodOp : public seed::PodOpLockerNoWrite<PodOp, DeviceMap::Locker> {
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
         if (static_cast<Tree*>(this->Sender_)->IoManager_->CreateDevice(*this->Seed_) == DeviceOpenResult::NewDeviceCreated) {
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
struct IoManager::Tree::TreeOp : public seed::TreeOp {
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
         DeviceMap::Locker  map{static_cast<Tree*>(&this->Tree_)->IoManager_->DeviceMap_};
         seed::MakeGridView(*map, seed::GetIteratorForGv(*map, req.OrigKey_),
                            req, res, &MakePolicyRecordView);
      } // unlock.
      fnCallback(res);
   }
   void Get(StrView strKeyText, seed::FnPodOp fnCallback) override {
      {
         DeviceMap::Locker  map{static_cast<Tree*>(&this->Tree_)->IoManager_->DeviceMap_};
         auto               ifind = seed::GetIteratorForPod(*map, strKeyText);
         if (ifind != map->end()) {
            PodOp op(**ifind, *static_cast<Tree*>(&this->Tree_), seed::OpResult::no_error, map);
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
         DeviceMap::Locker curmap{static_cast<Tree*>(&this->Tree_)->IoManager_->DeviceMap_};
         if (req.BeforeGrid_.begin() != nullptr) {
            seed::MakeGridView(*curmap, curmap->begin(), seed::GridViewRequestFull{*req.Tab_}, gvres, &MakePolicyRecordView);
            if (req.BeforeGrid_ != ToStrView(gvres.GridView_)) {
               res.OpResult_ = seed::OpResult::bad_apply_submit;
               resmsg = "Orig tree changed";
               goto __UNLOCK_AND_CALLBACK;
            }
         }
         IoManager::Apply(req.Tab_->Fields_, req.EditingGrid_, *curmap, oldmap);
      }  // unlock curmap.
      // After grid view apply. oldmap = removed items.
      if (!oldmap.empty()) {
         TimeStamp now = UtcNow();
         for (auto& i : oldmap) {
            i->DisposeDevice(now, "Removed from ApplySubmit");
            // StatusSubj_ 使用 [整表] 通知: seed::SeedSubj_TableChanged();
            // 所以這裡就不用樹發 PodRemoved() 事件了.
            // seed::SeedSubj_NotifyPodRemoved(static_cast<Tree*>(&this->Tree_)->StatusSubj_, this->Tree_, ToStrView(i->Id_));
         }
      }
      if (static_cast<Tree*>(&this->Tree_)->ConfigFileBinder_.HasBinding()) {
         RevBufferList rbuf{128};
         RevPutChar(rbuf, *fon9_kCSTR_ROWSPL);
         seed::RevPrintConfigFieldNames(rbuf, req.Tab_->Fields_, &this->Tree_.LayoutSP_->KeyField_->Name_);
         static_cast<Tree*>(&this->Tree_)->ConfigFileBinder_.WriteConfig(
            ToStrView("IoManager." + static_cast<Tree*>(&this->Tree_)->IoManager_->Name_),
            BufferTo<std::string>(rbuf.MoveOut()) + req.EditingGrid_.ToString());
      }
      static_cast<Tree*>(&this->Tree_)->StartTimerForOpen();

   __UNLOCK_AND_CALLBACK:
      fnCallback(res, resmsg);
      if (res.OpResult_ == seed::OpResult::no_error)
         seed::SeedSubj_TableChanged(static_cast<Tree*>(&this->Tree_)->StatusSubj_,
                                     this->Tree_, *this->Tree_.LayoutSP_->GetTab(kTabStatusIndex));
   }
   seed::OpResult Subscribe(SubConn* pSubConn, seed::Tab& tab, seed::SeedSubr subr) override {
      if (&tab != this->Tree_.LayoutSP_->GetTab(kTabStatusIndex))
         return this->SubscribeUnsupported(pSubConn);
      *pSubConn = static_cast<Tree*>(&this->Tree_)->StatusSubj_.Subscribe(subr);
      return seed::OpResult::no_error;
   }
   seed::OpResult Unsubscribe(SubConn pSubConn) override {
      return static_cast<Tree*>(&this->Tree_)->StatusSubj_.Unsubscribe(pSubConn)
         ? seed::OpResult::no_error : seed::OpResult::not_supported_subscribe;
   }
};
void IoManager::Tree::OnTreeOp(seed::FnTreeOp fnCallback) {
   TreeOp op{*this};
   fnCallback(seed::TreeOpResult{this, seed::OpResult::no_error}, &op);
}
void IoManager::Tree::OnTabTreeOp(seed::FnTreeOp fnCallback) {
   TreeOp op{*this};
   static_cast<seed::TabTreeOp*>(this->TabTreeOp_.get())->HandleTabTreeOp(op, std::move(fnCallback));
}

//--------------------------------------------------------------------------//
struct IoManager::AcceptedTree : public seed::Tree {
   fon9_NON_COPY_NON_MOVE(AcceptedTree);
   using base = seed::Tree;
   using Locker = io::AcceptedClients::ConstLocker;
   struct PodOp : public seed::PodOpDefault {
      fon9_NON_COPY_NON_MOVE(PodOp);
      using base = PodOpDefault;
      io::DeviceSP Device_;
      Locker&      Locker_;
      PodOp(io::Device* seed, Tree& sender, const StrView& key, Locker& locker)
         : base{sender, seed::OpResult::no_error, key}
         , Device_{seed}
         , Locker_(locker) {
      }
      void Lock() {
         if (!this->Locker_.owns_lock())
            this->Locker_.lock();
      }
      void Unlock() {
         if (this->Locker_.owns_lock())
            this->Locker_.unlock();
      }
      void BeginRead(seed::Tab& tab, seed::FnReadOp fnCallback) override {
         this->Lock();
         if (auto item = IoManager::FromManagerBookmark(*this->Device_))
            this->BeginRW(tab, std::move(fnCallback), seed::SimpleRawRd{*item});
         else
            base::BeginRead(tab, std::move(fnCallback));
      }
      void OnSeedCommand(seed::Tab* tab, StrView cmdln, seed::FnCommandResultHandler resHandler) override {
         this->Unlock();
         this->Tab_ = tab;
         std::string msg = this->Device_->DeviceCommand(cmdln);
         resHandler(*this, &msg);
      }
   };
   struct TreeOp : public seed::TreeOp {
      fon9_NON_COPY_NON_MOVE(TreeOp);
      using base = seed::TreeOp;
      TreeOp(Tree& tree) : base(tree) {
      }
      static void MakePolicyRecordView(io::AcceptedClientsImpl::const_iterator ivalue, seed::Tab* tab, RevBuffer& rbuf) {
         if (tab) {
            if (auto item = IoManager::FromManagerBookmark(**ivalue))
               FieldsCellRevPrint(tab->Fields_, seed::SimpleRawRd{*item}, rbuf, seed::GridViewResult::kCellSplitter);
         }
         RevPrint(rbuf, (*ivalue)->GetAcceptedClientSeq());
      }
      void GridView(const seed::GridViewRequest& req, seed::FnGridViewOp fnCallback) override {
         seed::GridViewResult res{this->Tree_, req.Tab_};
         {
            auto  map = static_cast<AcceptedTree*>(&this->Tree_)->Listener_->Lock();
            seed::MakeGridView(*map, seed::GetIteratorForGv(*map, req.OrigKey_),
                               req, res, &MakePolicyRecordView);
         } // unlock.
         fnCallback(res);
      }
      void Get(StrView strKeyText, seed::FnPodOp fnCallback) override {
         {
            auto  map = static_cast<AcceptedTree*>(&this->Tree_)->Listener_->Lock();
            auto  ifind = seed::GetIteratorForPod(*map, strKeyText);
            if (ifind != map->end()) {
               PodOp op(*ifind, this->Tree_, strKeyText, map);
               fnCallback(op, &op);
               return;
            }
         } // unlock.
         fnCallback(seed::PodOpResult{this->Tree_, seed::OpResult::not_found_key, strKeyText}, nullptr);
      }
   };
public:
   const io::DeviceListenerSP Listener_;
   AcceptedTree(io::DeviceListenerSP&& listener)
      : base{IoManager::MakeAcceptedClientLayout()}
      , Listener_{std::move(listener)} {
   }
   void OnTreeOp(seed::FnTreeOp fnCallback) override {
      TreeOp op{*this};
      fnCallback(seed::TreeOpResult{this, seed::OpResult::no_error}, &op);
   }
};
void IoManager::MakeAcceptedClientTree(DeviceRun& serItem, io::DeviceListenerSP listener) {
   if (!serItem.Sapling_ || static_cast<AcceptedTree*>(serItem.Sapling_.get())->Listener_ != listener)
      serItem.Sapling_.reset(new AcceptedTree{std::move(listener)});
}
fon9_WARN_POP;

//--------------------------------------------------------------------------//
bool IoManager::DeviceRun::DisposeDevice(TimeStamp now, StrView cause) {
   if (this->Device_) {
      // 應該在 if (e.State_ == io::State::Disposing) 時清除 ManagerBookmark,
      // 否則會造成 this leak.
      // this->Device_->SetManagerBookmark(0);
      this->Device_->AsyncDispose(cause.ToString());
      return false;
   }
   this->SessionSt_.clear();
   AssignStStr(this->DeviceSt_, now, cause);
   return true;
}
} // namespaces
