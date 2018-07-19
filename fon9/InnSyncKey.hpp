/// \file fon9/InnSyncKey.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_InnSyncKey_hpp__
#define __fon9_InnSyncKey_hpp__
#include "fon9/HostId.hpp"
#include "fon9/TimeStamp.hpp"
#include "fon9/Archive.hpp"

namespace fon9 {

/// \ingroup Inn
/// 用來判斷同步資料是否有效(是否為過期的舊資料? 是否為新的有效資料).
/// - 這裡沒有提供嚴謹的同步機制, 不能拿來當作交易使用.
/// - 僅使用 TimeStamp_ 來判斷資料資料的新舊
/// - 如果 TimeStamp_ 相同, 則判斷 ModifySeq_, HostId_
/// - 用於一般資料的同步處理, 例如: 使用者資料表、使用者政策、全部主機共用的設定...
///   - 這類資料操作, 一般只會在某台主機上進行, 然後經由簡易同步機制送給其他主機
struct InnSyncKey : public Comparable<InnSyncKey> {
   TimeStamp   ModifyTime_;
   uint32_t    ModifySeq_{0};
   HostId      OrigHostId_;

   InnSyncKey() = default;
   InnSyncKey(TimeStamp ts, HostId hostid)
      : ModifyTime_{ts}
      , OrigHostId_{hostid} {
   }

   /// 當本地資料異動後, 設定 SyncKey.
   /// 後續再由呼叫端: 連同異動的內容, 寫入同步資料.
   void SetLocalModified() {
      ++this->ModifySeq_;
      this->OrigHostId_ = LocalHostId_;
      TimeStamp now = UtcNow();
      if (fon9_LIKELY(this->ModifyTime_ < now))
         this->ModifyTime_ = now;
      else {
         // 本機現在時間有錯 or 上次異動的 Host 時間有錯.
         // 此次異動, 為了要讓同步生效(其他主機判斷此次異動為新):
         // (1) 如果 ModifySeq_ 比異動前大, 則不改變時間.
         // (2) 如果 ModifySeq_ 比異動前小(overflow), 則將時間增加一點點.
         // => 若 A,B 兩主機在尚未完成同步前同時異動,
         //    則使用 HostId_ 來決定使用 A 或 B 的版本.
         if (fon9_UNLIKELY(this->ModifySeq_ <= 0))
            this->ModifyTime_ += TimeInterval::Make<TimeInterval::Scale>(1);
      }
   }

   /// 檢查 rhs 是否為較新的資料.
   bool operator<(const InnSyncKey& rhs) const {
      if (fon9_LIKELY(this->ModifyTime_ < rhs.ModifyTime_))
         return true;
      if (fon9_LIKELY(this->ModifyTime_ > rhs.ModifyTime_))
         return false;
      if (fon9_LIKELY(this->ModifySeq_ < rhs.ModifySeq_))
         return true;
      if (fon9_LIKELY(this->ModifySeq_ > rhs.ModifySeq_))
         return false;
      return this->OrigHostId_ < rhs.OrigHostId_;
   }
   bool operator==(const InnSyncKey& rhs) const {
      return this->ModifyTime_ == rhs.ModifyTime_
         && this->ModifySeq_ == rhs.ModifySeq_
         && this->OrigHostId_ == rhs.OrigHostId_;
   }

   template <class Archive>
   inline friend void Serialize(Archive& ar, ArchiveWorker<Archive, InnSyncKey>& syncKey) {
      ar(syncKey.ModifyTime_, syncKey.ModifySeq_, syncKey.OrigHostId_);
   }
};

} // namespaces
#endif//__fon9_InnSyncKey_hpp__
