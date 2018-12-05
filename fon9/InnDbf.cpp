/// \file fon9/InnDbf.cpp
/// \author fonwinz@gmail.com
#include "fon9/InnDbf.hpp"
#include "fon9/BitvArchive.hpp"
#include "fon9/Log.hpp"
#include "fon9/DefaultThreadPool.hpp"
#include "fon9/buffer/DcQueueList.hpp"

namespace fon9 {

InnDbfTableHandler::~InnDbfTableHandler() {
}

//--------------------------------------------------------------------------//

static const char          kInnDbfExHeaderMsg[] = "InnDbf";
static const unsigned      kInnDbfExHeaderVer = 0;
static const InnRoomSize   kInnDbfExHeaderSize = 256;

InnDbf::OpenResult InnDbf::Open(OpenArgs& oargs) {
   try {
      InnDbf::OpenResult res = this->OpenImpl(oargs);
      if (!res)
         this->Close();
      return res;
   }
   catch (std::exception& e) {
      this->Close();
      fon9_LOG_ERROR("InnDbf.Open|dbf=", this->GetDbfName(), "|err=", e.what());
      return OpenResult{std::errc::bad_message};
   }
}
void InnDbf::OnNewInnFile() {
   this->ExHeaderLastRoomKey_ = this->InnFile_.MakeNewRoom(kInnRoomType_ExFileHeader, kInnDbfExHeaderSize);
   RevBufferFixedSize<64>  rbuf;
   // InnDbf ExHeader format: NextRoomPos + (Bitv)(kInnDbfExHeaderMsg + kInnDbfExHeaderVer)
   // ver=0: [TableId + TableName] * N
   ToBitv(rbuf, kInnDbfExHeaderVer);
   ToBitv(rbuf, kInnDbfExHeaderMsg);
   RevPutFill(rbuf, sizeof(InnFile::RoomPosT), '\0');
   this->InnFile_.Rewrite(this->ExHeaderLastRoomKey_, rbuf.GetCurrent(), static_cast<InnRoomSize>(rbuf.GetUsedSize()));
}
InnDbf::OpenResult InnDbf::OpenImpl(OpenArgs& oargs) {
   OpenResult res = this->InnFile_.Open(oargs);
   if (!res)
      return res;
   this->Clear();
   InnFile::RoomPosT next = 0;
   InnFile::RoomKey  rkey;
   BufferList        buf;
   do {
      rkey = this->InnFile_.MakeRoomKey(GetBigEndian<InnFile::RoomPosT>(&next), &next, sizeof(next));
      if (fon9_UNLIKELY(rkey.GetCurrentRoomType() != kInnRoomType_ExFileHeader)) {
         if (fon9_UNLIKELY(next == 0 && !rkey)) {
            this->OnNewInnFile();
            return res;
         }
         return OpenResult{std::errc::bad_message};
      }
      this->InnFile_.Read(rkey, sizeof(next), static_cast<InnRoomSize>(rkey.GetDataSize() - sizeof(next)), buf);
   } while (next != 0);
   this->ExHeaderLastRoomKey_ = std::move(rkey);

   DcQueueList dcbuf{std::move(buf)};
   if (dcbuf.empty())
      return OpenResult{std::errc::bad_message};

   InnDbfTableName   tableName;
   unsigned          ver = 0;
   BitvTo(dcbuf, tableName);
   BitvTo(dcbuf, ver);
   if (ToStrView(tableName) != kInnDbfExHeaderMsg || ver != kInnDbfExHeaderVer)
      return OpenResult{std::errc::bad_message};

   StrView           errmsg;
   TableMap::Locker  tableMap{this->TableMap_};
   InnDbfTableId     tableId = 0;

   while (!dcbuf.empty()) {
      BitvTo(dcbuf, tableId);
      BitvTo(dcbuf, tableName);
      if (fon9_UNLIKELY(tableId <= 0)) {
         errmsg = "Bad TableId";
      __RETURN_ERROR:
         fon9_LOG_ERROR("InnDbf.Open|table=", tableName, "|tableId=", tableId, "|err=", errmsg);
         return OpenResult{std::errc::bad_message};
      }
      InnDbfTableLinkSP table{new InnDbfTableLink(tableId, tableName, this)};
      auto& v = tableMap->kfetch(ToStrView(table->TableName_));
      if (v.second) {
         errmsg = "Dup tableName";
         goto __RETURN_ERROR;
      }
      if (tableMap->TableList_.size() < tableId)
         tableMap->TableList_.resize(tableId);
      else if (tableMap->TableList_[tableId - 1] != nullptr) {
         errmsg = "Dup tableId";
         goto __RETURN_ERROR;
      }
      tableMap->TableList_[tableId - 1] = table.get();
      v.second = std::move(table);
   }
   return res;
}

//--------------------------------------------------------------------------//

bool InnDbf::LinkTable(const StrView tableName, InnDbfTableHandler& handler, InnRoomSize minRoomSize) {
   if (handler.Table_)
      return false;
   InnDbfTableLink*  table;
   TableMap::Locker  tableMap{this->TableMap_};
   auto              ifind = tableMap->find(tableName);
   InnDbfTableLinkSP tableNeedsLoad;
   bool              isNewTable = false;
   if (fon9_LIKELY(ifind != tableMap->end())) {
      table = ifind->second.get();
      if (table->Handler_)
         return false;
      if (table->RoomCount_ > 0)
         tableNeedsLoad.reset(table);
   }
   else {
      const InnDbfTableId szTableList = static_cast<InnDbfTableId>(tableMap->TableList_.size());
      InnDbfTableId tableId = 0;
      while (tableId < szTableList) {
         if (tableMap->TableList_[tableId++] == nullptr)
            goto __FOUND_AVAILABLE_TABLEID;
      }
      tableMap->TableList_.resize(++tableId);
   __FOUND_AVAILABLE_TABLEID:
      InnDbfTableLinkSP tableSP{table = new InnDbfTableLink(tableId, InnDbfTableName{tableName}, this)};
      tableMap->insert(TableMapImpl::value_type{ToStrView(table->TableName_), std::move(tableSP)});
      tableMap->TableList_[tableId - 1] = table;
      isNewTable = true;
      tableNeedsLoad.reset(table);
   }
   handler.Table_.reset(table);
   table->Handler_ = &handler;
   table->MinRoomSize_ = minRoomSize;
   tableMap.unlock();

   if (isNewTable) { // Write Inn file ExHeader.
      AutoStartAsyncUpdateRequests autoAsync{*this};
      autoAsync.Requests_->emplace_back(UpdateRequest{std::move(tableNeedsLoad), InnDbfRoomSP{}});
   }
   else if (tableNeedsLoad) {
      this->LoadTable(tableNeedsLoad);
   }
   return true;
}
void InnDbf::LoadRoom(InnDbfTableHandler& handler, InnFile::RoomKey&& roomKey, DcQueue&& dcbuf) {
   InnDbfLoadEventArgs  loadArgs{InnDbfRoomKey{new InnDbfRoom{std::move(roomKey)}}, &dcbuf};
   BitvInArchive{dcbuf}(loadArgs.RoomKey_.RoomSP_->SyncKey_);
   handler.OnInnDbfTable_Load(loadArgs);
}
void InnDbf::LoadAll() {
   if (this->IsLoadedAll_)
      return;
   this->IsLoadedAll_ = true;
   InnFile::RoomKey roomKey = this->InnFile_.MakeRoomKey(0);
   if (!roomKey)
      return;
   StrView           errmsg;
   InnFile::RoomPosT roomPos = roomKey.GetNextRoomPos();
   for (;;) {
      roomKey = this->InnFile_.MakeRoomKey(roomPos);
      if (!roomKey)
         break;
      roomPos = roomKey.GetNextRoomPos();

      switch (roomKey.GetCurrentRoomType()) {
      case static_cast<InnRoomType>(InnDbfRoomType::RowData):
      case static_cast<InnRoomType>(InnDbfRoomType::RowDeleted):
         break;
      default: // ExHeader, or ...
         continue;
      case kInnRoomType_Free:
         FreedRoomsMap::Locker freedRoomsMap{this->FreedRoomsMap_};
         freedRoomsMap->kfetch(roomKey.GetRoomSize()).second.push_back(roomKey.GetRoomPos());
         continue;
      }

      BufferList buf;
      this->InnFile_.ReadAll(roomKey, buf);
      DcQueueList   dcbuf{std::move(buf)};
      InnDbfTableId tableId = 0;
      BitvTo(dcbuf, tableId);

      TableMap::Locker tableMap{this->TableMap_};
      if (fon9_UNLIKELY(tableId <= 0 || tableMap->TableList_.size() < tableId)) {
         errmsg = StrView{"Bad tableId"};
      __LOG_ERROR:
         fon9_LOG_ERROR("InnDbf.LoadAll|dbf=", this->GetDbfName(),
                        "|inn=", this->InnFile_.GetOpenName(),
                        "|pos=", roomKey.GetRoomPos(),
                        "|tableId=", tableId,
                        "|err=", errmsg);
         continue;
      }
      InnDbfTableLink*  table = tableMap->TableList_[tableId - 1];
      if (fon9_UNLIKELY(table == nullptr)) {
         errmsg = StrView{"table not found"};
         goto __LOG_ERROR;
      }

      ++table->RoomCount_;
      if (table->Handler_)
         this->LoadRoom(*table->Handler_, std::move(roomKey), std::move(dcbuf));
   }
   if (this->Syncer_)
      this->Syncer_->AttachHandler(this);
}
void InnDbf::LoadTable(InnDbfTableLinkSP table) {
   InnFile::RoomKey roomKey = this->InnFile_.MakeRoomKey(0);
   if (!roomKey)
      return;
   size_t            count = 0;
   InnFile::RoomPosT roomPos = roomKey.GetNextRoomPos();
   for (;;) {
      byte  dcmembuf[sizeof(InnDbfTableId)+2];
      roomKey = this->InnFile_.MakeRoomKey(roomPos, dcmembuf, sizeof(dcmembuf));
      if (!roomKey)
         break;
      roomPos = roomKey.GetNextRoomPos();
      if (roomKey.GetDataSize() <= sizeof(dcmembuf))
         continue;

      DcQueueFixedMem dcmem{dcmembuf, sizeof(dcmembuf)};
      InnDbfTableId   tableId = 0;
      BitvTo(dcmem, tableId);
      if (tableId != table->TableId_)
         continue;

      InnRoomSize usedsz = static_cast<InnRoomSize>(dcmem.Peek1() - dcmembuf);
      BufferList  buf;
      this->InnFile_.Read(roomKey, usedsz, roomKey.GetDataSize() - usedsz, buf);
      this->LoadRoom(*table->Handler_, std::move(roomKey), DcQueueList{std::move(buf)});
      if (++count >= table->RoomCount_)
         break;
   }
}

//--------------------------------------------------------------------------//

void InnDbf::Clear() {
   this->IsLoadedAll_ = false;
   this->IsUpdatingRequests_ = false;
   this->FreedRoomsMap_.Lock()->clear();
   this->TableMap_.Lock()->clear();
}
void InnDbf::Close() {
   if (this->Syncer_)
      this->Syncer_->DetachHandler(*this);
   this->WaitFlush();
   this->Clear();
   this->InnFile_.Close();
}
void InnDbf::DelinkTable(InnDbfTableHandler& handler) {
   TableMap::Locker tableMap{this->TableMap_};
   if (!handler.Table_)
      return;

   InnDbfTableId  tableIndex = handler.Table_->TableId_ - 1;
   if (tableIndex < tableMap->TableList_.size()
       && handler.Table_ == tableMap->TableList_[tableIndex]) {
      // table 並沒有從 dbf 移除, 只是移除 handler, 所以 TableList 必須留存.
      // tableMap->TableList_[tableIndex] = nullptr;
      handler.Table_->Handler_ = nullptr;
      handler.Table_.reset();
   }
}

//--------------------------------------------------------------------------//

void InnDbf::WaitFlush() {
   while (!this->DoUpdateRequests())
      std::this_thread::sleep_for(std::chrono::milliseconds{1});
}
void InnDbf::StartAsyncUpdateRequests() {
   GetDefaultThreadPool().EmplaceMessage(std::bind(&InnDbf::DoUpdateRequests, InnDbfSP{this}));
}
void InnDbf::WriteExHeaderAddTable(InnDbfTableLink& table) {
   RevBufferList rbuf{64};
   ToBitv(rbuf, table.TableName_);
   ToBitv(rbuf, table.TableId_);
   InnRoomSize requiredSize = static_cast<InnRoomSize>(CalcDataSize(rbuf.cfront()));
   if (this->ExHeaderLastRoomKey_.GetRemainSize() < requiredSize) {
      InnFile::RoomKey  rkey = this->AllocRoom(nullptr, kInnRoomType_ExFileHeader, requiredSize + kInnDbfExHeaderSize);
      InnFile::RoomPosT nextRoomPos;
      PutBigEndian(&nextRoomPos, rkey.GetRoomPos());
      this->InnFile_.Write(this->ExHeaderLastRoomKey_, 0, &nextRoomPos, sizeof(nextRoomPos));
      this->ExHeaderLastRoomKey_ = std::move(rkey);
      nextRoomPos = 0;
      RevPutMem(rbuf, &nextRoomPos, sizeof(nextRoomPos));
      requiredSize += static_cast<InnRoomSize>(sizeof(nextRoomPos));
   }
   DcQueueList dcbuf{rbuf.MoveOut()};
   this->InnFile_.Write(this->ExHeaderLastRoomKey_, this->ExHeaderLastRoomKey_.GetDataSize(), requiredSize, dcbuf);
}
bool InnDbf::DoUpdateRequests() {
   fon9_WARN_DISABLE_PADDING;
   struct Requests : public UpdateRequests::Locker {
      fon9_NON_COPY_NON_MOVE(Requests);
      InnDbf& Owner_;
      bool    IsUpdatingRequests_;

      Requests(InnDbf& owner)
         : UpdateRequests::Locker{owner.UpdateRequests_}
         , Owner_(owner)
         , IsUpdatingRequests_(!owner.IsUpdatingRequests_) {
      }
      ~Requests() {
         if (this->IsUpdatingRequests_)
            this->Owner_.IsUpdatingRequests_ = false;
      }
   };
   fon9_WARN_POP;
   Requests pendingRequests{*this};
   if (!pendingRequests.IsUpdatingRequests_)
      return false;
   this->IsUpdatingRequests_ = true;
   goto __INTO_CHECK_EMPTY;

   for (;;) {
      pendingRequests.lock();
__INTO_CHECK_EMPTY:
      if (pendingRequests->empty())
         return true;
      UpdateRequest  req(pendingRequests->front());
      pendingRequests->pop_front();
      if (!req.Room_) {
         pendingRequests.unlock();
         this->WriteExHeaderAddTable(*req.Table_);
         continue;
      }
      if (!req.Table_) {
         pendingRequests.unlock();
         FreedRoomsMap::Locker freedRoomsMap{this->FreedRoomsMap_};
         this->WriteFreeRoom(freedRoomsMap, req.Room_->RoomKey_);
         continue;
      }

      if (req.Room_->PendingRoomNote_ == InnDbfRoomNote::NoPending)
         continue;

      try {
         req.Room_->RoomKey_.SetPendingRoomType(static_cast<InnRoomType>(req.Room_->PendingRoomType_));
         req.Room_->RoomKey_.SetNote(static_cast<InnFile::RoomKey::NoteT>(req.Room_->PendingRoomNote_));
         req.Room_->PendingRoomNote_ = InnDbfRoomNote::NoPending;

         RevBufferList  rbuf{sizeof(InnDbfTableId), std::move(req.Room_->PendingWrite_)};
         BitvOutArchive oar{rbuf};
         oar(req.Room_->SyncKey_);

         // 到此已將要寫入的資料準備好了,
         // 所以可以安心的 unlock, 然後執行較花時間的 Write.
         pendingRequests.unlock();

         this->WriteSync(req, rbuf.cfront());
         oar(req.Table_->TableId_);

         DcQueueList       dcbuf{rbuf.MoveOut()};
         const InnRoomSize bufsz = static_cast<InnRoomSize>(dcbuf.CalcSize());
         if (bufsz > req.Room_->RoomKey_.GetRoomSize()) {
            if (!req.Room_->RoomKey_)
               ++req.Table_->RoomCount_;
            this->ReallocRoom(req.Room_->RoomKey_, std::max(req.Table_->MinRoomSize_, bufsz));
         }
         this->InnFile_.Rewrite(req.Room_->RoomKey_, dcbuf);
      }
      catch (std::exception& e) {
         fon9_LOG_FATAL("InnDbf.DoUpdateRequests|dbf=", this->GetDbfName(),
                        "|table=", req.Table_->TableName_,
                        "|room=", req.Room_->RoomKey_.GetRoomPos(),
                        "|err=", e.what());
      }
   }
}

//--------------------------------------------------------------------------//

void InnDbf::UpdateRoom(InnDbfTableLink& table,
                        InnDbfRoomKey& roomKey,
                        const InnSyncKey* syncKey,
                        InnDbfRoomType roomType,
                        BufferList&& buf) {
   DcQueueList cancelBuf;//在 autoAsync 解構(unlock)後, 取消沒用到的 buffer node.
   AutoStartAsyncUpdateRequests autoAsync{*this};
   InnDbfRoom* room = roomKey.RoomSP_.get();
   if (room == nullptr)
      roomKey.RoomSP_.reset(room = new InnDbfRoom{});
   if (room->PendingRoomNote_ == InnDbfRoomNote::NoPending)
      autoAsync.Requests_->emplace_back(UpdateRequest{&table, room});
   if (syncKey) {
      room->PendingRoomNote_ = InnDbfRoomNote::FromSyncer;
      room->SyncKey_ = *syncKey;
   }
   else {
      room->PendingRoomNote_ = InnDbfRoomNote::NeedsSync;
      room->SyncKey_.SetLocalModified();
   }
   room->PendingRoomType_ = roomType;
   room->PendingWrite_.swap(buf);
   cancelBuf.push_back(std::move(buf));
}
void InnDbf::FreeRoom(InnDbfTableLink& table, InnDbfRoomKey&& roomKey) {
   AutoStartAsyncUpdateRequests autoAsync{*this};
   --table.RoomCount_;
   autoAsync.Requests_->emplace_back(UpdateRequest{nullptr, std::move(roomKey.RoomSP_)});
}

void InnDbf::WriteFreeRoom(FreedRoomsMap::Locker& freedRoomsMap, InnFile::RoomKey& roomKey) {
   if (!roomKey)
      freedRoomsMap.unlock();
   else {
      freedRoomsMap->kfetch(roomKey.GetRoomSize()).second.push_back(roomKey.GetRoomPos());
      freedRoomsMap.unlock();
      this->InnFile_.FreeRoom(roomKey, kInnRoomType_Free);
   }
}

//--------------------------------------------------------------------------//

InnFile::RoomKey InnDbf::AllocRoom(InnFile::RoomKey* oldRoomKey, InnRoomType roomType, InnRoomSize requiredSize) {
   {
      FreedRoomsMap::Locker freedRoomsMap{this->FreedRoomsMap_};
      auto   ifind = freedRoomsMap->lower_bound(requiredSize);
      while (ifind != freedRoomsMap->end()) {
         if (requiredSize <= ifind->first) {
            InnFile::RoomPosT pos = ifind->second.back();
            ifind->second.pop_back();
            if (ifind->second.empty())
               freedRoomsMap->erase(ifind);
            if (oldRoomKey)
               this->WriteFreeRoom(freedRoomsMap, *oldRoomKey);
            else
               freedRoomsMap.unlock();
            return this->InnFile_.ReallocRoom(pos, roomType, requiredSize);
         }
         ++ifind;
      }
      if (oldRoomKey)
         this->WriteFreeRoom(freedRoomsMap, *oldRoomKey);
   } // unlock freedRoomsMap.
   return this->InnFile_.MakeNewRoom(roomType, requiredSize);
}
void InnDbf::ReallocRoom(InnFile::RoomKey& roomKey, InnFile::SizeT requiredSize) {
   if (roomKey.GetRoomSize() < requiredSize)
      roomKey = this->AllocRoom(&roomKey, roomKey.GetPendingRoomType(), requiredSize);
}

//--------------------------------------------------------------------------//

void InnDbf::WriteSync(const UpdateRequest& req, const BufferNode* node) {
   if (!this->Syncer_ || static_cast<InnDbfRoomNote>(req.Room_->RoomKey_.GetNote()) != InnDbfRoomNote::NeedsSync)
      return;
   RevBufferList  rbuf{static_cast<BufferNodeSize>(req.Table_->TableName_.size() + 64 + this->SyncHandlerName_.size())};
   RevPrint(rbuf, node);
   ToBitv(rbuf, req.Table_->TableName_);
   ToBitv(rbuf, req.Room_->RoomKey_.GetPendingRoomType());
   this->Syncer_->WriteSync(*this, std::move(rbuf));
}
void InnDbf::OnInnSyncReceived(InnSyncer& syncer, DcQueue&& buf) {
   (void)syncer;
   assert(this->Syncer_ == &syncer);
   InnDbfSyncEventArgs  evArgs;
   InnDbfTableName      tableName;
   BitvInArchive        iar{buf};
   iar(evArgs.RoomType_, tableName, evArgs.SyncKey_);

   InnDbfTableHandler*  handler;
   {
      TableMap::Locker  tables{this->TableMap_};
      auto ifind = tables->find(ToStrView(tableName));
      if (ifind == tables->end()) {
         fon9_LOG_WARN("InnDbf.OnInnSync|dbf=", this->GetDbfName(),
                       "|tableName=", tableName,
                       "|err=table not found");
         return;
      }
      evArgs.Buffer_ = &buf;
      handler = ifind->second->Handler_;
   } // unlock.
   if (handler)
      handler->OnInnDbfTable_Sync(evArgs);
   else
      fon9_LOG_WARN("InnDbf.OnInnSync|dbf=", this->GetDbfName(),
                    "|tableName=", tableName,
                    "|err=table handler not found");
}
void InnDbf::OnInnSyncFlushed(InnSyncer& syncer) {
   (void)syncer;
   assert(this->Syncer_ == &syncer);
   TableMap::Locker  tables{this->TableMap_};
   for (auto& table : *tables) {
      if (table.second->Handler_)
         table.second->Handler_->OnInnDbfTable_SyncFlushed();
   }
}

} // namespaces
