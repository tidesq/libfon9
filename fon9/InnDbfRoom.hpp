/// \file fon9/InnDbfRoom.hpp
///
/// ```
///  InnDbfRoom format:                   _____________________
///                                      /    Bitv format      \.
///  +-- 4bytes --+- 1 byte -+- 4bytes -+-----------+-----------+------------+
///  | BlockCount | RoomType | DataSize |  TableId  |  SyncKey  |  Payload   |
///  +------------+----------+----------+-----------+-----------+------------+
///   \______ InnFile RoomHeader ______/ \_ InnDbf RoomHeader _/
///
///
///  Sync data format:
///    ________________________________
///   /          Bitv format           \.
///  +----------+-----------+-----------+------------+
///  | RoomType | TableName |  SyncKey  |  Payload   |
///  +----------+-----------+-----------+------------+
///                          \_ Same as InnDbfRoom _/
///
///
///  Payload:
///    由 TableHandler 處理, 通常會加上 Version, 然後接著資料.
///    InnDbfRoomType::RowData:    儲存一筆完整資料.
///    InnDbfRoomType::RowDeleted: 通常僅儲存資料的Key, 用來判斷後續同步資料的新舊.
/// ```
///
/// \author fonwinz@gmail.com
#ifndef __fon9_InnDbfRoom_hpp__
#define __fon9_InnDbfRoom_hpp__
#include "fon9/InnSyncKey.hpp"
#include "fon9/InnFile.hpp"
#include "fon9/intrusive_ref_counter.hpp"
#include "fon9/buffer/BufferList.hpp"

namespace fon9 {

using InnDbfTableId = uint32_t;

class fon9_API InnDbf;
using InnDbfSP = intrusive_ptr<InnDbf>;

class fon9_API InnDbfTableLink;
using InnDbfTableLinkSP = intrusive_ptr<InnDbfTableLink>;

class fon9_API InnDbfTableHandler;

//--------------------------------------------------------------------------//

enum class InnDbfRoomType : InnRoomType {
   /// Room 裡面儲存的是一般的資料.
   RowData = 0x10,
   /// Room 裡面儲存的是一個被刪除的 RowKey.
   /// 用來避免收到舊的同步資料時, 被刪掉的舊資料又復活了.
   RowDeleted = 0x11,
};

enum class InnDbfRoomNote : InnFile::RoomKey::NoteT {
   NoPending,
   NeedsSync,
   FromSyncer,
};

fon9_WARN_DISABLE_PADDING;
/// \ingroup Inn
class InnDbfRoom : public intrusive_ref_counter<InnDbfRoom> {
   fon9_NON_COPY_NON_MOVE(InnDbfRoom);

   InnDbfRoomNote    PendingRoomNote_{InnDbfRoomNote::NoPending};
   InnDbfRoomType    PendingRoomType_;
   BufferList        PendingWrite_;

   InnFile::RoomKey  RoomKey_;
   InnSyncKey        SyncKey_;

   friend class InnDbf;
   friend class InnDbfTableLink;
   friend class InnDbfRoomKey;

   InnDbfRoom() = default;
   InnDbfRoom(InnFile::RoomKey&& roomKey)
      : RoomKey_{std::move(roomKey)} {
   }
   InnDbfRoom(InnFile::RoomKey&& roomKey, const InnSyncKey& syncKey)
      : RoomKey_{std::move(roomKey)}
      , SyncKey_{syncKey} {
   }
};
using InnDbfRoomSP = intrusive_ptr<InnDbfRoom>;
fon9_WARN_POP;

/// \ingroup Inn
class InnDbfRoomKey {
   fon9_NON_COPYABLE(InnDbfRoomKey);
   friend class InnDbf;
   InnDbfRoomSP   RoomSP_;
   InnDbfRoomKey(InnDbfRoomSP&& room) : RoomSP_{std::move(room)} {
   }
public:
   InnDbfRoomKey() = default;
   InnDbfRoomKey(InnDbfRoomKey&&) = default;
   InnDbfRoomKey& operator=(InnDbfRoomKey&&) = default;

   explicit operator bool() const {
      return this->RoomSP_.get() != nullptr;
   }
   void swap(InnDbfRoomKey& rhs) {
      this->RoomSP_.swap(rhs.RoomSP_);
   }

   InnDbfRoomType GetRoomType() const {
      return static_cast<InnDbfRoomType >(this->RoomSP_->RoomKey_.GetPendingRoomType());
   }
   const InnSyncKey& GetSyncKey() const {
      return this->RoomSP_->SyncKey_;
   }
};

} // namespaces
#endif//__fon9_InnDbfRoom_hpp__
