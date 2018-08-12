// \file fon9/PolicyTable.cpp
// \author fonwinz@gmail.com
#include "fon9/auth/PolicyTable.hpp"
#include "fon9/seed/Tree.hpp"
#include "fon9/BitvArchive.hpp"

namespace fon9 { namespace auth {

PolicyItem::~PolicyItem() {
}
void PolicyItem::SetRemoved(PolicyTable&) {
}
void PolicyItem::OnParentTreeClear(PolicyTable& owner) {
   this->BeforeParentErase(owner);
}
void PolicyItem::OnSeedCommand(PolicyMaps::Locker&, seed::SeedOpResult& res, StrView cmdln, seed::FnCommandResultHandler resHandler) {
   (void)cmdln;
   res.OpResult_ = seed::OpResult::not_supported_cmd;
   resHandler(res, nullptr);
}
seed::TreeSP PolicyItem::GetSapling() {
   return seed::TreeSP{};
}

//--------------------------------------------------------------------------//

void PolicyMapsImpl::WriteUpdated(PolicyItem& rec) {
   if (rec.IsRemoved_)
      return;
   if (!rec.RoomKey_) {
      auto ifind = this->DeletedMap_.find(rec.PolicyId_);
      if (ifind != this->DeletedMap_.end()) {
         rec.RoomKey_ = std::move(ifind->second);
         this->DeletedMap_.erase(ifind);
      }
   }
   RevBufferList  rbuf{128};
   rec.SavePolicy(rbuf);
   ToBitv(rbuf, rec.PolicyId_);
   PolicyTable::StaticCast(this->ItemMap_).WriteRoom(rec.RoomKey_, nullptr, fon9::InnDbfRoomType::RowData, rbuf.MoveOut());
}

bool PolicyTable::Delete(StrView policyId) {
   PolicyMaps::Locker maps{this->PolicyMaps_};
   auto               irec = maps->ItemMap_.find(policyId);
   if (irec == maps->ItemMap_.end())
      return false;
   auto& droom = maps->DeletedMap_[(*irec)->PolicyId_];
   assert(!droom);
   droom = std::move((*irec)->RoomKey_);
   maps->ItemMap_.erase(irec);

   RevBufferList rbuf{64};
   ToBitv(rbuf, policyId);
   this->WriteRoom(droom, nullptr, InnDbfRoomType::RowDeleted, rbuf.MoveOut());
   return true;
}

//--------------------------------------------------------------------------//

void PolicyTable::LoadHandler::UpdateLoad(PolicyItemMap& itemMap, PolicyItemMap::iterator* iItem) {
   PolicyItem&  rec = this->FetchPolicy(PolicyTable::StaticCast(itemMap), itemMap, iItem);
   rec.RoomKey_ = std::move(this->EvArgs_.RoomKey_);
   rec.LoadPolicy(*this->EvArgs_.Buffer_);
}
void PolicyTable::LoadHandler::UpdateLoad(PolicyDeletedMap& deletedMap, PolicyDeletedMap::iterator* iDeleted) {
   if (iDeleted)
      (*iDeleted)->second = std::move(this->EvArgs_.RoomKey_);
   else
      deletedMap[this->Key_] = std::move(this->EvArgs_.RoomKey_);
}
void PolicyTable::OnInnDbfTable_Load(InnDbfLoadEventArgs& e) {
   LoadHandler handler{*this, e};
   BitvTo(*e.Buffer_, handler.Key_);

   PolicyMaps::Locker maps{this->PolicyMaps_};
   OnInnDbfLoad(maps->ItemMap_, maps->DeletedMap_, handler);
}

//--------------------------------------------------------------------------//

void PolicyTable::SyncHandler::UpdateSync(PolicyItemMap& itemMap, PolicyItemMap::iterator* iItem) {
   assert(this->EvArgs_.RoomType_ == InnDbfRoomType::RowData);
   PolicyItem&  rec = this->FetchPolicy(PolicyTable::StaticCast(itemMap), itemMap, iItem);
   if (iItem == nullptr)
      rec.RoomKey_ = std::move(this->PendingFreeRoomKey_);
   rec.LoadPolicy(*this->EvArgs_.Buffer_);
   rec.SavePolicy(this->PendingWriteBuf_);
   ToBitv(this->PendingWriteBuf_, rec.PolicyId_);
   this->PendingWriteRoomKey_ = &rec.RoomKey_;
}
void PolicyTable::SyncHandler::UpdateSync(PolicyDeletedMap& deletedMap, PolicyDeletedMap::iterator* iDeleted) {
   assert(this->EvArgs_.RoomType_ == InnDbfRoomType::RowDeleted);
   if (iDeleted)
      this->PendingWriteRoomKey_ = &((*iDeleted)->second);
   else {
      this->PendingWriteRoomKey_ = &deletedMap[this->Key_];
      *this->PendingWriteRoomKey_ = std::move(this->PendingFreeRoomKey_);
   }
   ToBitv(this->PendingWriteBuf_, this->Key_);
}
void PolicyTable::OnInnDbfTable_Sync(InnDbfSyncEventArgs& e) {
   SyncHandler handler{*this, e};
   BitvTo(*e.Buffer_, handler.Key_);

   PolicyMaps::Locker maps{this->PolicyMaps_};
   OnInnDbfSync(maps->ItemMap_, maps->DeletedMap_, handler);
   if (handler.PendingWriteRoomKey_)
      this->WriteRoom(*handler.PendingWriteRoomKey_, &e.SyncKey_, e.RoomType_, handler.PendingWriteBuf_.MoveOut());
}

//--------------------------------------------------------------------------//

void PolicyTable::OnInnDbfTable_SyncFlushed() {
   PolicyDeletedMap deletedMap;
   {
      PolicyMaps::Locker maps{this->PolicyMaps_};
      deletedMap.swap(maps->DeletedMap_);
   }
   for (auto& v : deletedMap)
      this->FreeRoom(std::move(v.second));
}

} } // namespaces
