/// \file fon9/framework/IoManager.cpp
/// \author fonwinz@gmail.com
#include "fon9/framework/IoManager.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/Log.hpp"

namespace fon9 {

IoManager::IoManager(const IoManagerArgs& args)
   : Name_{args.Name_}
   , SessionFactoryPark_{args.SessionFactoryPark_ ? args.SessionFactoryPark_ : new SessionFactoryPark{"SessionFactoryPark"}}
   , DeviceFactoryPark_{args.DeviceFactoryPark_ ? args.DeviceFactoryPark_ : new DeviceFactoryPark{"DeviceFactoryPark"}}
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
      RevPrint(rbuf, "IoManager.CreateIoService|name=", this->Name_, "info=use default config");
      LogWrite(LogLevel::Error, std::move(rbuf));
   }
}

void IoManager::RaiseMakeIoServiceFatal(Result2 err) const {
   std::string errmsg = RevPrintTo<std::string>("IoManager.MakeIoService|name=", this->Name_, "err=", err);
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

bool IoManager::CreateDevice(DeviceItem& item) {
   if (item.Device_)
      return true;
   item.SessionSt_.clear();
   item.DeviceSt_.clear();
   auto sesFactory = this->SessionFactoryPark_->Get(ToStrView(item.Config_.SessionName_));
   if (!sesFactory) {
      item.SessionSt_.assign("Session factory not found.");
      return false;
   }
   if (auto devFactory = this->DeviceFactoryPark_->Get(ToStrView(item.Config_.DeviceName_))) {
      if (!(item.Device_ = devFactory->CreateDevice(this, *sesFactory, item.Config_))) {
         item.DeviceSt_.assign("IoManager.CreateDevice|err=cannot create this device.");
         return false;
      }
      item.Device_->SetManagerBookmark(reinterpret_cast<io::Bookmark>(&item));
      item.Device_->Initialize();
      return true;
   }
   item.DeviceSt_.assign("Device factory not found");
   return false;
}
bool IoManager::AddConfig(StrView id, const IoConfigItem& cfg) {
   DeviceItemSP item{new DeviceItem};
   item->Id_.assign(id);
   item->Config_ = cfg;

   DeviceMap::Locker map{this->DeviceMap_};
   if (!map->insert(item).second)
      return false;
   if (cfg.Enabled_ == EnabledYN::Yes) {
      if (this->CreateDevice(*item)) {
         if (cfg.DeviceArgs_.empty())
            // 沒提供 DeviceArgs_, 則由使用者 or Session 手動開啟.
            item->DeviceSt_.assign("Waiting for manual open");
         else {
            item->DeviceSt_.assign("Async opening");
            item->Device_->AsyncOpen(cfg.DeviceArgs_.ToString());
         }
      }
   }
   return true;
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
   fon9_LOG_TRACE("IoManager.OnDevice_Destructing|dev=", ToPtr(&dev));
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
   RevPutChar(rbuf, '\n');
   if (auto item = this->FromManagerBookmark(dev)) {
      RevPrint(rbuf,
               "|st=", GetStateStr(e.State_),
               "|id={", e.DeviceId_, "}"
               "|info=", e.Info_);
      const BufferNode* bnode = rbuf.cfront();
      char* pmsg = static_cast<char*>(item->DeviceSt_.alloc(kDateTimeStrWidth + CalcDataSize(bnode)));
      ToStrRev(pmsg += kDateTimeStrWidth, UtcNow());
      while (bnode) {
         auto datsz = bnode->GetDataSize();
         memcpy(pmsg, bnode->GetDataBegin(), datsz);
         pmsg += datsz;
         bnode = bnode->GetNext();
      }
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
   }
   // fon9_LOG_TRACE:
   if (fon9_UNLIKELY(LogLevel::Trace >= fon9::LogLevel_)) {
      RevPrint(rbuf, "IoManager.DeviceState|dev=", ToPtr(&dev));
      LogWrite(LogLevel::Trace, std::move(rbuf));
   }
}
void IoManager::OnSession_StateUpdated(io::Device& dev, StrView stmsg) {
   if (io::DeviceAcceptedClient* cli = dynamic_cast<io::DeviceAcceptedClient*>(&dev)) {
      auto clis = cli->Owner_->Lock();
      this->UpdateSessionStateLocked(dev, stmsg);
   }
   else {
      DeviceMap::Locker map{this->DeviceMap_};
      this->UpdateSessionStateLocked(dev, stmsg);
   }
}
void IoManager::UpdateSessionStateLocked(io::Device& dev, StrView stmsg) {
   if (auto item = this->FromManagerBookmark(dev)) {
      size_t sz = kDateTimeStrWidth + stmsg.size() + 1;
      // 底下 -+sizeof(NumOutBuf) 是因為 RevPrint() 需要分配較多的保留大小.
      // 但因我確定實際大小不會超過, 所以自行調整.
      RevBufferFixedMem rbuf{static_cast<char*>(item->SessionSt_.alloc(sz)) - sizeof(NumOutBuf), sz + sizeof(NumOutBuf)};
      RevPrint(rbuf, UtcNow(), '|', stmsg);
   }
   fon9_LOG_INFO("IoManager.SessionState|dev=", ToPtr(&dev), "|st=", stmsg);
}

//--------------------------------------------------------------------------//

seed::LayoutSP IoManager::MakeAcceptedClientLayout() {
   struct Aux {
      static seed::LayoutSP MakeLayout() {
         seed::Fields fields;
         fields.Add(fon9_MakeField(Named{"SessionSt"}, DeviceRun, SessionSt_));
         fields.Add(fon9_MakeField(Named{"DeviceSt"},  DeviceRun, DeviceSt_));
         fields.Add(fon9_MakeField(Named{"OpenArgs"},  DeviceRun, OpenArgs_));
         seed::TabSP tabSt{new seed::Tab(Named{"Status"}, std::move(fields))};
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
         seed::TabSP tabConfig{new seed::Tab(Named{"Config"}, std::move(fields), saplingLayout)};

         return new seed::LayoutN(fon9_MakeField(Named{"Id"}, DeviceItem, Id_),
                                  std::move(tabSt),
                                  std::move(tabConfig));
      }
   };
   static seed::LayoutSP Layout_{Aux::MakeLayoutImpl()};
   return Layout_;
}
void IoManager::Tree::OnParentSeedClear() {
   // dispose all devices.
   DeviceMap::Locker map{this->IoManager_->DeviceMap_};
   for (auto& i : *map) {
      if (io::Device* dev = i->Device_.get())
         dev->AsyncDispose("Parent clear");
   }
}

//--------------------------------------------------------------------------//

fon9_WARN_DISABLE_PADDING;
fon9_MSC_WARN_DISABLE_NO_PUSH(4265 /* class has virtual functions, but destructor is not virtual. */
                              4355 /* 'this' : used in base member initializer list*/);
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
      io::DeviceSP dev = this->Seed_->Device_;
      if(!dev) {
         // cmd = "open" => 建立 device & open.
         return;
      }
      locker.unlock();
      std::string msg = dev->DeviceCommand(cmdln);
      resHandler(res, &msg);
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
};
void IoManager::Tree::OnTreeOp(seed::FnTreeOp fnCallback) {
   TreeOp op{*this};
   fnCallback(seed::TreeOpResult{this, seed::OpResult::no_error}, &op);
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

} // namespaces
