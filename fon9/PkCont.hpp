// \file fon9/PkCont.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_PkCont_hpp__
#define __fon9_PkCont_hpp__
#include "fon9/SortedVector.hpp"
#include "fon9/Timer.hpp"

namespace fon9 {

/// \ingroup Misc.
/// 確保收到封包的連續性.
/// - 可能有多個資訊源, 但序號相同.
/// - 即使同一個資訊源封包也可能亂序.
/// - 若封包序號小於期望, 視為重複封包, 拋棄之.
/// - 若封包序號不連續, 則等候一小段時間, 若仍沒收到連續封包, 則放棄等候.
class fon9_API PkContFeeder {
   fon9_NON_COPY_NON_MOVE(PkContFeeder);
public:
   using SeqT = uint64_t;

   PkContFeeder();
   virtual ~PkContFeeder();

   /// 收到的封包透過這裡處理.
   /// - 若封包有連續, 則透過 PkContOnReceived() 通知衍生者.
   /// - 若封包不連續:
   ///   - 若 this->NextSeq_ == 0, 則直接轉 this->PkContOnReceived(); 由衍生者處理.
   ///   - 等候一小段時間(this->WaitInterval_), 若無法取得連續封包, 則強制繼續處理.
   void FeedPacket(const void* pk, unsigned pksz, SeqT seq);

   void Clear();

protected:
   SeqT           NextSeq_{0};
   SeqT           ReceivedCount_{0};
   SeqT           DroppedCount_{0};
   TimeInterval   WaitInterval_{TimeInterval_Millisecond(5)};
   
   struct PkRec : public std::string {
      SeqT  Seq_;
      PkRec(SeqT seq) : Seq_{seq} {
      }
      bool operator<(const PkRec& rhs) const {
         return this->Seq_ < rhs.Seq_;
      }
   };
   using PkPendingsImpl = SortedVectorSet<PkRec>;
   using PkPendings = MustLock<PkPendingsImpl>;
   PkPendings  PkPendings_;

   virtual void PkContOnTimer();
   static void EmitOnTimer(TimerEntry* timer, TimeStamp now);
   DataMemberEmitOnTimer<&PkContFeeder::EmitOnTimer> Timer_;

   /// 通知衍生者收到的封包:
   /// - 當收到連續封包時.
   /// - 當超過指定時間沒有連續時.
   /// - 此時的 this->NextSeq_ 尚未改變, 仍維持前一次的預期序號.
   ///   因此可以判斷 this->NextSeq_ == seq 表示序號連續.
   /// - 為了確保資料連續性, 呼叫此處時會將 this->PkPendings_ 鎖住.
   ///   所以不會在不同 thread 重複進入 PkContOnReceived();
   virtual void PkContOnReceived(const void* pk, unsigned pksz, SeqT seq) = 0;
};

} // namespaces
#endif//__fon9_PkCont_hpp__
