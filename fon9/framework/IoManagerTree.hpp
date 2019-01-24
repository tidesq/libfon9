/// \file fon9/framework/IoManagerTree.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_framework_IoManagerTree_hpp__
#define __fon9_framework_IoManagerTree_hpp__
#include "fon9/framework/IoManager.hpp"
#include "fon9/ConfigFileBinder.hpp"

namespace fon9 {

fon9_WARN_DISABLE_PADDING;
class fon9_API IoManagerTree : public seed::Tree, public IoManager {
   fon9_NON_COPY_NON_MOVE(IoManagerTree);
   using baseTree = seed::Tree;
   unsigned IoManagerAddRef() override;
   unsigned IoManagerRelease() override;
public:
   IoManagerTree(const IoManagerArgs& args);
   ~IoManagerTree();
   void OnTreeOp(seed::FnTreeOp fnCallback) override;
   void OnTabTreeOp(seed::FnTreeOp fnCallback) override;
   void OnParentSeedClear() override;

   template <class IoTree = IoManagerTree, class... ArgsT>
   static IoTree* Plant(seed::MaTree& maTree, const IoManagerArgs& ioargs, ArgsT&&... args) {
      intrusive_ptr<IoTree> retval{new IoTree(ioargs, std::forward<ArgsT>(args)...)};
      seed::NamedSapling*   seed;
      if (!maTree.Add(seed = new seed::NamedSapling(retval, ioargs.Name_)))
         return nullptr;
      seed->SetDescription(ioargs.Result_);
      return retval.get();
   }
private:
   struct TreeOp;
   struct PodOp;
   static seed::LayoutSP MakeLayoutImpl();
   static seed::LayoutSP GetLayout();
   seed::TreeSP      TabTreeOp_; // for NeedsApply.
   seed::SeedSubj    StatusSubj_;
   ConfigFileBinder  ConfigFileBinder_;
   SubConn           SubConnDev_;
   SubConn           SubConnSes_;
   friend class IoManager;
   void NotifyChanged(DeviceItem&) override;
   void NotifyChanged(DeviceRun&) override;

   static void EmitOnTimer(TimerEntry* timer, TimeStamp now);
   using Timer = DataMemberEmitOnTimer<&IoManagerTree::EmitOnTimer>;
   Timer Timer_{GetDefaultTimerThread()};
   enum class TimerFor {
      Open,
      CheckSch,
   };
   TimerFor TimerFor_;
   // 啟動Timer: n秒後檢查: Open or Close devices.
   void StartTimerForOpen();
   void OnFactoryParkChanged();

   static void Apply(const seed::Fields& flds, StrView src, DeviceMapImpl& curmap, DeviceMapImpl& oldmap);
};
fon9_WARN_POP;

} // namespaces
#endif//__fon9_framework_IoManagerTree_hpp__
