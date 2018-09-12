/// \file fon9/framework/Framework.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_framework_Framework_hpp__
#define __fon9_framework_Framework_hpp__
#include "fon9/seed/MaTree.hpp"
#include "fon9/auth/AuthMgr.hpp"

namespace fon9 {

/// \ingroup Misc
struct fon9_API Framework {
   std::string       ConfigPath_;
   std::string       SyncerPath_;
   seed::MaTreeSP    Root_;
   InnSyncerSP       Syncer_;
   auth::AuthMgrSP   MaAuth_;

   ~Framework();

   /// create system default object.
   /// - 設定工作目錄:  `-w dir` or `--workdir dir`
   /// - ConfigPath_ = `-c cfgpath` or `--cfg cfgpath` or `getenv("fon9cfg");` or default="fon9cfg"
   /// - 然後載入設定: fon9local.cfg, fon9common.cfg; 設定內容包含:
   ///   - LogFileFmt  如果沒設定, log 就輸出在 console.
   ///     - $LogFileFmt=./logs/{0:f+'L'}/fon9sys-{1:04}.log  # 超過 {0:f+'L'}=YYYYMMDD(localtime), {1:04}=檔案序號.
   ///     - $LogFileSizeMB=n                                 # 超過 n MB 就換檔.
   ///   - $HostId     沒有預設值, 如果沒設定, 就不會設定 LocalHostId_
   ///   - $SyncerPath 指定 InnSyncerFile 的路徑, 預設 = "fon9syn"
   ///   - $MaAuthName 預設 "MaAuth"
   ///   - $MemLock    預設 "N"
   void Initialize(int argc, char** argv);

   /// dbf.LoadAll(), syncer.StartSync(), ...
   void Start();

   /// syncer.StopSync(), dbf.Close(), root.OnParentSeedClear(), ...
   void Dispose();

   /// Dispose() 並且等候相關 thread 執行完畢.
   void DisposeForAppQuit();
};

} // namespaces
#endif//__fon9_framework_Framework_hpp__
