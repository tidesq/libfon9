/// \file fon9/InnDbfTable.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_InnDbfTable_hpp__
#define __fon9_InnDbfTable_hpp__
#include "fon9/InnDbfRoom.hpp"
#include "fon9/CharVector.hpp"
#include "fon9/buffer/RevBufferList.hpp"

namespace fon9 {

using InnDbfTableName = CharVector;

fon9_WARN_DISABLE_PADDING;
class fon9_API InnDbfTableLink : public intrusive_ref_counter<InnDbfTableLink> {
   fon9_NON_COPY_NON_MOVE(InnDbfTableLink);
public:
   const InnDbfTableId     TableId_;
   const InnDbfTableName   TableName_;
   const InnDbfSP          OwnerDbf_;

   InnDbfTableLink(InnDbfTableId tableId, InnDbfTableName tableName, InnDbfSP ownerDbf)
      : TableId_{tableId}
      , TableName_{std::move(tableName)}
      , OwnerDbf_{std::move(ownerDbf)} {
   }

   InnDbf& GetOwnerDbf() const {
      return *this->OwnerDbf_;
   }

   size_t GetRoomCount() const {
      return this->RoomCount_;
   }

private:
   friend class InnDbf;
   InnDbfTableHandler*  Handler_{nullptr};
   InnRoomSize          MinRoomSize_{0};
   size_t               RoomCount_{0};
};

//--------------------------------------------------------------------------//

struct InnDbfLoadEventArgs {
   fon9_NON_COPY_NON_MOVE(InnDbfLoadEventArgs);

   InnDbfRoomKey  RoomKey_;
   DcQueue*       Buffer_;

   InnDbfLoadEventArgs(InnDbfRoomKey&& roomKey, DcQueue* buf)
      : RoomKey_{std::move(roomKey)}
      , Buffer_{buf} {
   }
};
struct InnDbfSyncEventArgs {
   InnDbfRoomType RoomType_;
   InnSyncKey     SyncKey_;
   DcQueue*       Buffer_;
};

class fon9_API InnDbfTableHandler {
   friend class InnDbf;
   InnDbfTableLinkSP Table_;
public:
   virtual ~InnDbfTableHandler();
   virtual void OnInnDbfTable_Load(InnDbfLoadEventArgs& e) = 0;
   virtual void OnInnDbfTable_Sync(InnDbfSyncEventArgs& e) = 0;
   virtual void OnInnDbfTable_SyncFlushed() = 0;

   bool IsDbfLinked() const {
      return this->Table_.get() != nullptr;
   }
   void WriteRoom(InnDbfRoomKey& roomKey,
                  const InnSyncKey* syncKey,
                  InnDbfRoomType roomType,
                  BufferList&& buf);
   void FreeRoom(InnDbfRoomKey&& roomKey);

   /// 僅供參考, 此值不包含「非同步尚未完成的 WriteRoom()」.
   size_t GetRoomCount() const {
      return this->Table_ ? this->Table_->GetRoomCount() : 0;
   }
};
fon9_WARN_POP;

//--------------------------------------------------------------------------//

enum class InnSyncCheckResult {
   NotExists,
   SyncIsNewer,
   SyncIsOlder,
};

template <class Map, class Iterator, class HandlerBase>
inline InnSyncCheckResult InnSyncCheck(Map& map, Iterator& ifind, const InnSyncKey& syncKey, HandlerBase& handler) {
   if ((ifind = map.find(handler.Key_)) == map.end())
      return InnSyncCheckResult::NotExists;
   if (syncKey <= handler.GetRoomKey(*ifind).GetSyncKey())
      return InnSyncCheckResult::SyncIsOlder;
   return InnSyncCheckResult::SyncIsNewer;
}

struct InnDbfLoadHandler {
   fon9_NON_COPY_NON_MOVE(InnDbfLoadHandler);
   InnDbfTableHandler&  Handler_;
   InnDbfLoadEventArgs& EvArgs_;
   InnDbfRoomKey        PendingFreeRoomKey1_;
   InnDbfRoomKey        PendingFreeRoomKey2_;
   InnDbfLoadHandler(InnDbfTableHandler& handler, InnDbfLoadEventArgs& e)
      : Handler_(handler)
      , EvArgs_(e) {
   }
   ~InnDbfLoadHandler() {
      if (this->PendingFreeRoomKey1_)
         this->Handler_.FreeRoom(std::move(this->PendingFreeRoomKey1_));
      if (this->PendingFreeRoomKey2_)
         this->Handler_.FreeRoom(std::move(this->PendingFreeRoomKey2_));
   }
};

template <class Map, class LoadHandler>
InnSyncCheckResult OnInnDbfLoad_Erase(Map& map, LoadHandler& handler) {
   typename Map::iterator  imap;
   InnSyncCheckResult      res = InnSyncCheck(map, imap, handler.EvArgs_.RoomKey_.GetSyncKey(), handler);
   switch (res) {
   case InnSyncCheckResult::NotExists:
      break;
   case InnSyncCheckResult::SyncIsNewer:
      handler.PendingFreeRoomKey1_ = std::move(handler.GetRoomKey(*imap));
      map.erase(imap);
      break;
   case InnSyncCheckResult::SyncIsOlder:
      handler.PendingFreeRoomKey1_ = std::move(handler.EvArgs_.RoomKey_);
      break;
   }
   return res;
}

template <class Map, class LoadHandler>
InnSyncCheckResult OnInnDbfLoad_Update(Map& map, LoadHandler& handler) {
   typename Map::iterator  imap;
   InnSyncCheckResult      res = InnSyncCheck(map, imap, handler.EvArgs_.RoomKey_.GetSyncKey(), handler);
   switch (res) {
   default:
   case InnSyncCheckResult::NotExists:
      handler.UpdateLoad(map, static_cast<typename Map::iterator*>(nullptr));
      break;
   case InnSyncCheckResult::SyncIsNewer:
      handler.PendingFreeRoomKey2_ = std::move(handler.GetRoomKey(*imap));
      handler.UpdateLoad(map, &imap);
      break;
   case InnSyncCheckResult::SyncIsOlder:
      handler.PendingFreeRoomKey2_ = std::move(handler.EvArgs_.RoomKey_);
      break;
   }
   return res;
}

/// \ingroup Inn
/// 協助 InnDbfTableHandler::OnInnDbfTable_Load(InnDbfLoadEventArgs& e); 處理載入資料.
/// \code
///   struct HandlerBase {
///      KeyType  Key_;
///      static InnDbfRoomKey& GetRoomKey(MainContainer::value_type&);
///      static InnDbfRoomKey& GetRoomKey(DeletedContainer::value_type&);
///   };
///   struct LoadHandler : public HandlerBase, public fon9::InnDbfLoadHandler {
///      fon9_NON_COPY_NON_MOVE(LoadHandler);
///      using InnDbfLoadHandler::InnDbfLoadHandler;
///      void UpdateLoad(MainMap&    mainMap,    MainMap::iterator*    iMain);
///      void UpdateLoad(DeletedMap& deletedMap, DeletedMap::iterator* iDeleted);
///   };
/// \endcode
template <class MainMap, class DeletedMap, class LoadHandler>
void OnInnDbfLoad(MainMap& mainMap, DeletedMap& deletedMap, LoadHandler& handler) {
   switch (handler.EvArgs_.RoomKey_.GetRoomType()) {
   case InnDbfRoomType::RowDeleted:
      if (OnInnDbfLoad_Erase(mainMap, handler) != InnSyncCheckResult::SyncIsOlder)
         OnInnDbfLoad_Update(deletedMap, handler);
      break;
   case InnDbfRoomType::RowData:
      if (OnInnDbfLoad_Erase(deletedMap, handler) != InnSyncCheckResult::SyncIsOlder)
         OnInnDbfLoad_Update(mainMap, handler);
      break;
   }
}

//--------------------------------------------------------------------------//

struct InnDbfSyncHandler {
   fon9_NON_COPY_NON_MOVE(InnDbfSyncHandler);
   InnDbfTableHandler&  Handler_;
   InnDbfSyncEventArgs& EvArgs_;
   InnDbfRoomKey        PendingFreeRoomKey_;
   InnDbfSyncHandler(InnDbfTableHandler& handler, InnDbfSyncEventArgs& e)
      : Handler_(handler)
      , EvArgs_(e) {
   }
   ~InnDbfSyncHandler() {
      if (this->PendingFreeRoomKey_)
         this->Handler_.FreeRoom(std::move(this->PendingFreeRoomKey_));
   }
   template <class Map>
   static void BeforeErase(Map& map, typename Map::iterator imap) {
      (void)map; (void)imap;
   }
};

template <class Map, class SyncHandler>
InnSyncCheckResult OnInnDbfSync_Erase(Map& map, SyncHandler& handler) {
   typename Map::iterator  imap;
   InnSyncCheckResult      res = InnSyncCheck(map, imap, handler.EvArgs_.SyncKey_, handler);
   switch (res) {
   case InnSyncCheckResult::NotExists:
      break;
   case InnSyncCheckResult::SyncIsNewer:
      handler.PendingFreeRoomKey_ = std::move(handler.GetRoomKey(*imap));
      handler.BeforeErase(map, imap);
      map.erase(imap);
      break;
   case InnSyncCheckResult::SyncIsOlder:
      break;
   }
   return res;
}

template <class Map, class SyncHandler>
InnSyncCheckResult OnInnDbfSync_Update(Map& map, SyncHandler& handler) {
   typename Map::iterator  imap;
   InnSyncCheckResult      res = InnSyncCheck(map, imap, handler.EvArgs_.SyncKey_, handler);
   switch (res) {
   default:
   case InnSyncCheckResult::NotExists:
      handler.UpdateSync(map, static_cast<typename Map::iterator*>(nullptr));
      break;
   case InnSyncCheckResult::SyncIsNewer:
      handler.UpdateSync(map, &imap);
      break;
   case InnSyncCheckResult::SyncIsOlder:
      break;
   }
   return res;
}

/// \ingroup Inn
/// 協助 InnDbfTableHandler::OnInnDbfTable_Sync(InnDbfSyncEventArgs& e); 處理載入資料.
/// \code
///   struct HandlerBase {
///      KeyType  Key_;
///      static InnDbfRoomKey& GetRoomKey(MainContainer::value_type&);
///      static InnDbfRoomKey& GetRoomKey(DeletedContainer::value_type&);
///   };
///   struct SyncHandler : public HandlerBase, public fon9::InnDbfSyncHandler {
///      fon9_NON_COPY_NON_MOVE(SyncHandler);
///      using InnDbfSyncHandler::InnDbfSyncHandler;
///      void UpdateSync(MainMap&    mainMap,    MainMap::iterator*    iMain);
///      void UpdateSync(DeletedMap& deletedMap, DeletedMap::iterator* iDeleted);
///   };
/// \endcode
template <class MainMap, class DeletedMap, class SyncHandler>
void OnInnDbfSync(MainMap& mainMap, DeletedMap& deletedMap, SyncHandler& handler) {
   switch (handler.EvArgs_.RoomType_) {
   case InnDbfRoomType::RowDeleted:
      if (OnInnDbfSync_Erase(mainMap, handler) != InnSyncCheckResult::SyncIsOlder)
         OnInnDbfSync_Update(deletedMap, handler);
      break;
   case InnDbfRoomType::RowData:
      if (OnInnDbfSync_Erase(deletedMap, handler) != InnSyncCheckResult::SyncIsOlder)
         OnInnDbfSync_Update(mainMap, handler);
      break;
   }
}

} // namespaces
#endif//__fon9_InnDbfTable_hpp__
