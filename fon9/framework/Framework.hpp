/// \file fon9/framework/Framework.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_framework_Framework_hpp__
#define __fon9_framework_Framework_hpp__
#include "fon9/seed/MaTree.hpp"
#include "fon9/auth/AuthMgr.hpp"

namespace fon9 {

struct fon9_API Framework {
   std::string       ConfigPath_ = "fon9cfg/";
   std::string       SyncerPath_ = "fon9cfg/sync/";
   seed::MaTreeSP    Root_;
   InnSyncerSP       Syncer_;
   auth::AuthMgrSP   MaAuth_;

   ~Framework();

   /// create system default object.
   void Initialize(int argc, char** argv);

   /// dbf.LoadAll(), syncer.StartSync(), ...
   void Start();

   /// syncer.StopSync(), dbf.Close(), root.OnParentSeedClear(), ...
   void Dispose();
};

} // namespaces
#endif//__fon9_framework_Framework_hpp__
