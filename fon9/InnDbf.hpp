/// \file fon9/InnDbf.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_InnDbf_hpp__
#define __fon9_InnDbf_hpp__
#include "fon9/InnDbfTable.hpp"
#include "fon9/InnSyncer.hpp"
#include <deque>

namespace fon9 {

fon9_WARN_DISABLE_PADDING;
/// \ingroup Inn
/// InnDbf 是一群 Table 集中儲存的地方.
/// - 啟動:
///   - Open(): 開啟 Dbf 檔, 載入此檔已存在的 table name/id 對照, 尚未載入 row data.
///   - LinkTable(): 建立/聯繫 table 資料處理者, 用來解析儲存在 dbf 裡(或來自 syncer), 各 table 的 row data.
///                  - 若尚未呼叫 LoadAll() 則會在 LoadAll() 時載入.
///                  - 若已呼叫 LoadAll(), 則會立即載入.
///   - LoadAll(): 載入各 table 的 row data.
///   - 啟動 Syncer.
/// - 結束:
///   - 關閉/停止 Syncer: 若沒有先停止 Syncer 則同步資料可能會遺失.
///   - DelinkTable()
///   - Close()
class fon9_API InnDbf : public InnSyncHandler {
   fon9_NON_COPY_NON_MOVE(InnDbf);
public:
   InnDbf(const StrView& dbfName, InnSyncerSP syncer)
      : InnSyncHandler{dbfName}
      , Syncer_{std::move(syncer)} {
   }

   const CharVector& GetDbfName() const {
      return this->SyncHandlerName_;
   }

   using OpenResult = InnFile::OpenResult;
   using OpenArgs = InnFile::OpenArgs;
   /// \copydoc InnFile::Open()
   OpenResult Open(OpenArgs&);
   OpenResult Open(std::string fileName) {
      OpenArgs oargs{std::move(fileName)};
      return this->Open(oargs);
   }

   /// \retval false tableName 已經有 handler.
   /// \retval true  成功與 tableName 建立關聯,
   ///               若已經呼叫過 LoadAll(); 則在返回前會自動載入.
   ///               handler 必須在死亡前主動呼叫 DelinkTable();
   bool LinkTable(StrView tableName, InnDbfTableHandler& handler, InnRoomSize minRoomSize);
   /// Open() => LinkTable() 之後, 載入全部的 rooms.
   /// 只能在 Open() 之後呼叫一次, 若尚未開啟則會拋出異常!
   /// 載入完成後, 會建立與 Syncer 的關聯.
   void LoadAll();

   /// 通常在載入模組提前卸載時會呼叫此處.
   /// 表示不再處理寫入、同步, 之後的同步訊息都將丟失!
   void DelinkTable(InnDbfTableHandler& handler);

   void Close();

   size_t TableCount() const {
      TableMap::Locker tableMap{const_cast<InnDbf*>(this)->TableMap_};
      return tableMap->size();
   }

   /// - syncKey == nullptr: 表示次異動來自本機操作員.
   /// - syncKey != nullptr: 表示次異動來自 Syncer.
   void UpdateRoom(InnDbfTableLink& table,
                   InnDbfRoomKey& roomKey,
                   const InnSyncKey* syncKey,
                   InnDbfRoomType roomType,
                   BufferList&& buf);
   void FreeRoom(InnDbfTableLink& table, InnDbfRoomKey&& roomKey);

   void WaitFlush();

private:
   bool              IsLoadedAll_{false};
   bool              IsUpdatingRequests_{false};
   const InnSyncerSP Syncer_;
   InnFile           InnFile_;
   InnFile::RoomKey  ExHeaderLastRoomKey_;

   using RoomList = std::vector<InnFile::RoomPosT>;
   using FreedRoomsImpl = SortedVector<InnFile::SizeT, RoomList>;
   using FreedRoomsMap = MustLock<FreedRoomsImpl>;
   FreedRoomsMap     FreedRoomsMap_;

   struct TableMapImpl : public SortedVector<StrView, InnDbfTableLinkSP> {
      using base = SortedVector<StrView, InnDbfTableLinkSP>;
      std::vector<InnDbfTableLink*> TableList_;
      void clear() {
         base::clear();
         this->TableList_.clear();
      }
   };
   using TableMap = MustLock<TableMapImpl>;
   TableMap          TableMap_;

   struct UpdateRequest {
      InnDbfTableLinkSP Table_;
      InnDbfRoomSP      Room_;
   };
   using UpdateRequestsImpl = std::deque<UpdateRequest>;
   using UpdateRequests = MustLock<UpdateRequestsImpl>;
   UpdateRequests UpdateRequests_;
   struct AutoStartAsyncUpdateRequests {
      fon9_NON_COPY_NON_MOVE(AutoStartAsyncUpdateRequests);
      bool                    IsNeedsAsync_{false};
      InnDbf&                 Owner_;
      UpdateRequests::Locker  Requests_;
      AutoStartAsyncUpdateRequests(InnDbf& owner)
         : Owner_(owner)
         , Requests_{owner.UpdateRequests_} {
         this->IsNeedsAsync_ = this->Requests_->empty() && !owner.IsUpdatingRequests_;
      }
      ~AutoStartAsyncUpdateRequests() {
         if (this->Requests_.owns_lock())
            this->Requests_.unlock();
         if (this->IsNeedsAsync_)
            this->Owner_.StartAsyncUpdateRequests();
      }
   };

   OpenResult OpenImpl(OpenArgs&);
   void OnNewInnFile();
   void WriteExHeaderAddTable(InnDbfTableLink& table);

   void StartAsyncUpdateRequests();
   bool DoUpdateRequests();
   void WriteSync(const UpdateRequest& req, const BufferNode*);
   void OnInnSyncReceived(InnSyncer& sender, DcQueue&& buf) override;
   void OnInnSyncFlushed(InnSyncer& sender) override;

   InnFile::RoomKey AllocRoom(InnFile::RoomKey* oldRoomKey, InnRoomType roomType, InnRoomSize requiredSize);
   void ReallocRoom(InnFile::RoomKey& roomKey, InnFile::SizeT requiredSize);
   void WriteFreeRoom(FreedRoomsMap::Locker& roomsMap, InnFile::RoomKey& roomKey);

   void Clear();
   void LoadTable(InnDbfTableLinkSP table);
   static void LoadRoom(InnDbfTableHandler& handler, InnFile::RoomKey&& roomKey, DcQueue&& buf);
};
fon9_WARN_POP;

inline void InnDbfTableHandler::WriteRoom(InnDbfRoomKey& roomKey,
                                          const InnSyncKey* syncKey,
                                          InnDbfRoomType roomType,
                                          BufferList&& buf) {
   this->Table_->GetOwnerDbf().UpdateRoom(*this->Table_, roomKey, syncKey, roomType, std::move(buf));
}
inline void InnDbfTableHandler::FreeRoom(InnDbfRoomKey&& roomKey) {
   this->Table_->GetOwnerDbf().FreeRoom(*this->Table_, std::move(roomKey));
}

} // namespaces
#endif//__fon9_InnDbf_hpp__
