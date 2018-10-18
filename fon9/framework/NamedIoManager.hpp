/// \file fon9/framework/NamedIoManager.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_framework_NamedIoManager_hpp__
#define __fon9_framework_NamedIoManager_hpp__
#include "fon9/framework/IoManager.hpp"

namespace fon9 {

class fon9_API NamedIoManager : public seed::NamedSapling {
   fon9_NON_COPY_NON_MOVE(NamedIoManager);
   using base = seed::NamedSapling;
public:
   NamedIoManager(IoManagerArgs& args)
      : base(new IoManager::Tree{args}, args.Name_, args.CfgFileName_) {
      this->SetDescription(args.Result_);
   }
   ~NamedIoManager();

   IoManager& GetIoManager() const {
      return *static_cast<IoManager::Tree*>(this->Sapling_.get())->IoManager_;
   }

   static IoManagerSP GetIoManagerFrom(seed::MaTree& root, StrView name) {
      if (auto mgr = root.Get<NamedIoManager>(name))
         return &mgr->GetIoManager();
      return nullptr;
   }
};

} // namespaces
#endif//__fon9_framework_NamedIoManager_hpp__
