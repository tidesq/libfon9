/// \file fon9/PolicyTable.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_auth_PolicyTable_hpp__
#define __fon9_auth_PolicyTable_hpp__
#include "fon9/auth/AuthBase.hpp"
#include "fon9/InnDbf.hpp"
#include <map>

namespace fon9 { namespace auth {

class fon9_API PolicyTable;

fon9_WARN_DISABLE_PADDING;
/// \ingroup auth
/// 一筆使用者政策設定的基底.
/// 例: Trader交易台股的權限.
struct PolicyItem : public intrusive_ref_counter<PolicyItem> {
   fon9_NON_COPY_NON_MOVE(PolicyItem);
   friend class PolicyTable;
   bool           IsRemoved_{false};
   InnDbfRoomKey  RoomKey_;

   virtual void LoadPolicy(DcQueue&) = 0;
   virtual void SavePolicy(RevBuffer&) = 0;
   virtual void SetRemoved(PolicyTable& owner) = 0;
   void BeforeParentErase(PolicyTable& owner) {
      this->IsRemoved_ = true;
      this->SetRemoved(owner);
   }
public:
   const PolicyId PolicyId_;
   PolicyItem(const StrView& policyId)
      : PolicyId_{policyId} {
   }
   virtual ~PolicyItem();

   /// 若傳回 true, 則表示 this 已從 table 移除.
   bool IsRemoved() const {
      return this->IsRemoved_;
   }

   /// 預設呼叫 BeforeParentErase(owner);
   virtual void OnParentTreeClear(PolicyTable& owner);
   virtual void OnSeedCommand(seed::SeedOpResult& res, StrView cmd, seed::FnCommandResultHandler resHandler);
   virtual seed::TreeSP GetSapling();
};
fon9_WARN_POP;
using PolicyItemSP = intrusive_ptr<PolicyItem>;

/// \ingroup auth
/// 使用者政策資料表, 負責與 InnDbf 溝通.
class fon9_API PolicyTable : public InnDbfTableHandler {
   fon9_NON_COPY_NON_MOVE(PolicyTable);
public:
   PolicyTable() = default;

   bool Delete(StrView policyId);

protected:
   virtual PolicyItemSP MakePolicy(const StrView& policyId) = 0;

   struct ItemMap : public std::map<StrView, PolicyItemSP> {
      using base = std::map<StrView, PolicyItemSP>;

      using base::find;
      iterator find(const PolicyId& key) { return this->find(ToStrView(key)); }

      using base::lower_bound;
      iterator lower_bound(const PolicyId& key) { return this->lower_bound(ToStrView(key)); }

      /// 移除前呼叫: i->second->BeforeParentErase(table);
      iterator erase(iterator i);

      std::pair<iterator, bool> insert(PolicyItemSP v) {
         return base::insert(value_type{ToStrView(v->PolicyId_), v});
      }
      mapped_type& operator[](const key_type&) = delete;
   };
   using DeletedMap = std::map<PolicyId, InnDbfRoomKey>;

   struct MapsImpl {
      ItemMap     ItemMap_;
      DeletedMap  DeletedMap_;

      void WriteUpdated(PolicyItem& rec);
   };
   using Maps = MustLock<MapsImpl>;
   Maps  Maps_;

private:
   struct HandlerBase {
      PolicyId Key_;
      static InnDbfRoomKey& GetRoomKey(DeletedMap::value_type& v) {
         return v.second;
      }
      static InnDbfRoomKey& GetRoomKey(ItemMap::value_type& v) {
         return v.second->RoomKey_;
      }
      PolicyItem& FetchPolicy(PolicyTable& table, ItemMap& itemMap, ItemMap::iterator* iItem) {
         if (iItem)
            return *(*iItem)->second;
         PolicyItemSP rec = table.MakePolicy(ToStrView(this->Key_));
         return *(itemMap.insert(std::move(rec)).first->second);
      }
   };

   struct LoadHandler : public HandlerBase, public InnDbfLoadHandler {
      fon9_NON_COPY_NON_MOVE(LoadHandler);
      using InnDbfLoadHandler::InnDbfLoadHandler;
      void UpdateLoad(ItemMap& itemMap, ItemMap::iterator* iItem);
      void UpdateLoad(DeletedMap& deletedMap, DeletedMap::iterator* iDeleted);
   };
   virtual void OnInnDbfTable_Load(InnDbfLoadEventArgs& e) override;

   struct SyncHandler : public HandlerBase, public InnDbfSyncHandler {
      fon9_NON_COPY_NON_MOVE(SyncHandler);
      SyncHandler(InnDbfTableHandler& handler, InnDbfSyncEventArgs& e)
         : InnDbfSyncHandler(handler, e) {
      }

      InnDbfRoomKey* PendingWriteRoomKey_{nullptr};
      RevBufferList  PendingWriteBuf_{128};

      void UpdateSync(ItemMap& itemMap, ItemMap::iterator* iItem);
      void UpdateSync(DeletedMap& deletedMap, DeletedMap::iterator* iDeleted);
   };
   virtual void OnInnDbfTable_Sync(InnDbfSyncEventArgs& e) override;
   virtual void OnInnDbfTable_SyncFlushed() override;
};

inline PolicyTable::ItemMap::iterator PolicyTable::ItemMap::erase(iterator i) {
   PolicyTable& table = ContainerOf(PolicyTable::Maps::StaticCast(ContainerOf(*this, &MapsImpl::ItemMap_)),
                                    &PolicyTable::Maps_);
   i->second->BeforeParentErase(table);
   return base::erase(i);
}

} } // namespaces
#endif//__fon9_auth_PolicyTable_hpp__
