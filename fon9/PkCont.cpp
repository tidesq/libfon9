// \file fon9/PkCont.cpp
// \author fonwinz@gmail.com
#include "fon9/PkCont.hpp"

namespace fon9 {

PkContFeeder::PkContFeeder() {
}
PkContFeeder::~PkContFeeder() {
   this->Timer_.StopAndWait();
}
void PkContFeeder::Clear() {
   this->Timer_.StopAndWait();
   PkPendings::Locker pks{this->PkPendings_};
   pks->clear();
   this->ReceivedCount_ = 0;
   this->DroppedCount_ = 0;
   this->NextSeq_ = 0;
}
void PkContFeeder::EmitOnTimer(TimerEntry* timer, TimeStamp now) {
   (void)now;
   PkContFeeder& rthis = ContainerOf(*static_cast<decltype(PkContFeeder::Timer_)*>(timer), &PkContFeeder::Timer_);
   rthis.PkContOnTimer();
}
void PkContFeeder::PkContOnTimer() {
   PkPendings::Locker pks{this->PkPendings_};
   for (const auto& i : *pks) {
      this->PkContOnReceived(i.data(), static_cast<unsigned>(i.size()), i.Seq_);
      this->NextSeq_ = i.Seq_ + 1;
      ++this->ReceivedCount_;
   }
   pks->clear();
}
void PkContFeeder::FeedPacket(const void* pk, unsigned pksz, SeqT seq) {
   {  // lock this->PkPendings_;
      PkPendings::Locker pks{this->PkPendings_};
      if (fon9_LIKELY(seq == this->NextSeq_)) {
__PK_RECEIVED:
         this->PkContOnReceived(pk, pksz, seq);
         this->NextSeq_ = seq + 1;
         ++this->ReceivedCount_;
         if (fon9_LIKELY(pks->empty()))
            return;
         auto const ibeg = pks->begin();
         auto const iend = pks->end();
         auto i = ibeg;
         while (i->Seq_ == this->NextSeq_) {
            this->PkContOnReceived(i->data(), static_cast<unsigned>(i->size()), i->Seq_);
            ++this->NextSeq_;
            ++this->ReceivedCount_;
            if (++i == iend)
               break;
         }
         if (ibeg != i)
            pks->erase(ibeg, i);
         return;
      }
      if (seq < this->NextSeq_) {
         ++this->DroppedCount_;
         return;
      }
      if (this->NextSeq_ == 0 || this->WaitInterval_.GetOrigValue() == 0)
         goto __PK_RECEIVED;
      bool isNeedsRunAfter = pks->empty();
      auto ires = pks->insert(PkRec{seq});
      if (!ires.second)
         return;
      ires.first->assign(static_cast<const char*>(pk), pksz);
      if (!isNeedsRunAfter)
         return;
   } // auto unlock this->PkPendings_.
   this->Timer_.RunAfter(this->WaitInterval_);
}

} // namespaces
