/// \file fon9/InnSyncerFile.hpp
///
///  Sync data in file:
///    +------ 4 bytes ------+--- uint64_t ---+- N bytes -+
///    | 0xff 0xff 0xff 0xff | Sync data size | Sync data |
///    +---------------------+----------------+-----------+
///                            big endian
///
/// \author fonwinz@gmail.com
#ifndef __fon9_InnSyncerFile_hpp__
#define __fon9_InnSyncerFile_hpp__
#include "fon9/InnSyncer.hpp"
#include "fon9/TimeInterval.hpp"
#include <memory> // std::unique_ptr

namespace fon9 {

fon9_WARN_DISABLE_PADDING;
/// \ingroup Inn
/// 使用檔案處理同步訊息.
class fon9_API InnSyncerFile : public InnSyncer {
   fon9_NON_COPY_NON_MOVE(InnSyncerFile);
   using base = InnSyncer;
   class Impl;
   std::unique_ptr<Impl> Impl_;
   virtual void WriteSyncImpl(RevBufferList&& rbuf) override;

public:
   struct CreateArgs {
      std::string    SyncOutFileName_;
      std::string    SyncInFileName_;
      TimeInterval   SyncInInterval_;
      mutable State  Result_{};

      CreateArgs() = default;
      CreateArgs(std::string syncOutFileName, std::string syncInFileName, TimeInterval syncInInterval)
         : SyncOutFileName_{std::move(syncOutFileName)}
         , SyncInFileName_{std::move(syncInFileName)}
         , SyncInInterval_{syncInInterval} {
      }
   };
   /// 若建構失敗會用 fon9_LOG_ERROR() 記錄原因, 並設定 State_ = State::ErrorCtor
   InnSyncerFile(const CreateArgs& args);
   ~InnSyncerFile();

   virtual State StartSync() override;
   virtual void StopSync() override;
};
fon9_WARN_POP;

} // namespaces
#endif//__fon9_InnSyncerFile_hpp__
