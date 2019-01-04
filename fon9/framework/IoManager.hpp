/// \file fon9/framework/IoManager.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_framework_IoManager_hpp__
#define __fon9_framework_IoManager_hpp__
#include "fon9/framework/IoFactory.hpp"
#include "fon9/ConfigFileBinder.hpp"
#include "fon9/SchTask.hpp"

#ifdef fon9_WINDOWS
#include "fon9/io/win/IocpService.hpp"
#else
#include "fon9/io/FdrService.hpp"
#endif

namespace fon9 {

fon9_WARN_DISABLE_PADDING;
struct IoManagerArgs {
   std::string Name_;
   
   /// 設定檔, 如果為空, 表示不使用設定檔.
   std::string CfgFileName_;

   /// 若 IoServiceSrc_ != nullptr 則不理會 IoServiceCfgstr_;
   /// 直接與 IoServiceSrc_ 共用 io service.
   IoManagerSP IoServiceSrc_;
   std::string IoServiceCfgstr_;
   
   /// 如果 SessionFactoryPark_ == nullptr, 則 IoManager 會自己建立一個.
   SessionFactoryParkSP SessionFactoryPark_;
   /// 如果 DeviceFactoryPark_ == nullptr, 則 IoManager 會自己建立一個.
   DeviceFactoryParkSP  DeviceFactoryPark_;

   /// 提供 IoManager 建構結果, 例如: 提供 ConfigFileBinder_.OpenRead(...args.CfgFileName_); 的錯誤訊息.
   std::string Result_;
};

class fon9_API IoManager : public io::Manager {
   fon9_NON_COPY_NON_MOVE(IoManager);

   struct DeviceRun;
   struct DeviceItem;

   /// 在首次有需要時(GetIocpService() or GetFdrService()), 才會將該 io service 建立起來.
#ifdef __fon9_io_win_IocpService_hpp__
   private:
      io::IocpServiceSP IocpService_;
      // Windows 也可能會有 FdrService_? 或其他種類的 io service?
   public:
      io::IocpServiceSP GetIocpService();
#endif

#ifdef __fon9_io_FdrService_hpp__
   private:
      io::FdrServiceSP  FdrService_;
      // Linux/Unix 可能會有其他種類的 io service?
   public:
      io::FdrServiceSP GetFdrService();
#endif

public:
   const std::string          Name_;
   const SessionFactoryParkSP SessionFactoryPark_;
   const DeviceFactoryParkSP  DeviceFactoryPark_;

   class Tree : public seed::Tree {
      fon9_NON_COPY_NON_MOVE(Tree);
      using base = seed::Tree;
      struct TreeOp;
      struct PodOp;
      static seed::LayoutSP MakeLayout();
      seed::TreeSP      TabTreeOp_; // for NeedsApply.
      seed::SeedSubj    StatusSubj_;
      ConfigFileBinder  ConfigFileBinder_;
      SubConn           SubConnDev_;
      SubConn           SubConnSes_;
      friend class IoManager;
      void NotifyChanged(DeviceItem&);
      void NotifyChanged(DeviceRun&);

      static void EmitOnTimer(TimerEntry* timer, TimeStamp now);
      using Timer = DataMemberEmitOnTimer<&Tree::EmitOnTimer>;
      Timer Timer_{GetDefaultTimerThread()};
      enum class TimerFor {
         Open,
         CheckSch,
      };
      TimerFor TimerFor_;
      // 啟動Timer: n秒後檢查: Open or Close devices.
      void StartTimerForOpen();
      void OnFactoryParkChanged();
   public:
      const IoManagerSP  IoManager_;
      Tree(IoManagerArgs& args);
      ~Tree();
      void OnTreeOp(seed::FnTreeOp fnCallback) override;
      void OnTabTreeOp(seed::FnTreeOp fnCallback) override;
      void OnParentSeedClear() override;
   };

   IoManager(Tree& ownerTree, const IoManagerArgs& args);

   /// 若 id 重複, 則返回 false, 表示失敗.
   /// 若 cfg.Enabled_ == EnabledYN::Yes 則會啟用該設定.
   /// - 若有 bind config file, 透過這裡加入設定, 不會觸發更新設定檔.
   ///   在透過 TreeOp 增刪改, 才會觸發寫入設定檔.
   bool AddConfig(StrView id, const IoConfigItem& cfg);

private:
   Tree*       OwnerTree_;
   std::string IoServiceCfgstr_;

   struct DeviceRun {
      io::DeviceSP   Device_;
      CharVector     SessionSt_;
      CharVector     DeviceSt_;
      seed::TreeSP   Sapling_;
      CharVector     OpenArgs_;

      /// - 如果此值==0 表示:
      ///   - this->Device_ 是從 DeviceItem 設定建立而來.
      ///   - 若 this->Device_ 為 DeviceServer, 則 this->Sapling_ 為 AcceptedClients tree.
      /// - 如果此值!=0 表示:
      ///   - this->Device_ 是 Accepted Client.
      ///   - this->Sapling_ 為顯示此 AcceptedClients 的 tree.
      io::DeviceAcceptedClientSeq   AcceptedClientSeq_{0};
      bool IsDeviceItem() const {
         return this->AcceptedClientSeq_ == 0;
      }
      /// return DeviceSt_ changed?
      bool DisposeDevice(TimeStamp now, StrView cause);
   };
   static DeviceRun* FromManagerBookmark(io::Device& dev);

   using DeviceItemId = CharVector;
   struct DeviceItem : public DeviceRun, public intrusive_ref_counter<DeviceItem> {
      SchSt          SchSt_{SchSt::Unknown};
      SchConfig      Sch_;
      DeviceItemId   Id_;
      IoConfigItem   Config_;

      DeviceItem(StrView id) : Id_{id} {
      }
      DeviceItem(StrView id, const IoConfigItem& cfg) : Id_{id}, Config_(cfg) {
      }
   };
   using DeviceItemSP = intrusive_ptr<DeviceItem>;

   struct CmpDeviceItemSP {
      bool operator()(const DeviceItemSP& lhs, const DeviceItemSP& rhs) const { return lhs->Id_ < rhs->Id_; }
      bool operator()(const DeviceItemSP& lhs, const StrView& rhs) const { return ToStrView(lhs->Id_) < rhs; }
      bool operator()(const StrView& lhs, const DeviceItemSP& rhs) const { return lhs < ToStrView(rhs->Id_); }
      bool operator()(const DeviceItemId& lhs, const DeviceItemSP& rhs) const { return lhs < rhs->Id_; }
      bool operator()(const DeviceItemSP& lhs, const DeviceItemId& rhs) const { return lhs->Id_ < rhs; }
   };
   using DeviceMapImpl = SortedVectorSet<DeviceItemSP, CmpDeviceItemSP>;
   using DeviceMap = MustLock<DeviceMapImpl>;
   DeviceMap   DeviceMap_;

   template <class IoService>
   intrusive_ptr<IoService> MakeIoService() const;

   void RaiseMakeIoServiceFatal(Result2 err) const;
   void ParseIoServiceArgs(io::IoServiceArgs& iosvArgs) const;

   void OnDevice_Accepted(io::DeviceServer& server, io::DeviceAcceptedClient& client) override;
   void OnDevice_Initialized(io::Device& dev) override;
   void OnDevice_Destructing(io::Device& dev) override;
   void OnDevice_StateChanged(io::Device& dev, const io::StateChangedArgs& e) override;
   void OnDevice_StateUpdated(io::Device& dev, const io::StateUpdatedArgs& e) override;
   void OnSession_StateUpdated(io::Device& dev, StrView stmsg, LogLevel lv) override;
   void UpdateDeviceState(io::Device& dev, const io::StateUpdatedArgs& e);
   void UpdateDeviceStateLocked(io::Device& dev, const io::StateUpdatedArgs& e);
   void UpdateSessionStateLocked(io::Device& dev, StrView stmsg, LogLevel lv);

   enum class DeviceOpenResult {
      NewDeviceCreated = 0,
      AlreadyExists,
      Disabled,
      SessionFactoryNotFound,
      DeviceFactoryNotFound,
      DeviceCreateError,
   };
   DeviceOpenResult CheckOpenDevice(DeviceItem& item);
   DeviceOpenResult CreateDevice(DeviceItem& item);
   static seed::LayoutSP MakeAcceptedClientLayout();
   static void MakeAcceptedClientTree(DeviceRun& serItem, io::DeviceListenerSP listener);
   struct AcceptedTree;

   static void Apply(const seed::Fields& flds, StrView src, DeviceMapImpl& curmap, DeviceMapImpl& oldmap);
};
fon9_WARN_POP;

} // namespaces
#endif//__fon9_framework_IoManager_hpp__
