// \file fon9/PkCont_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "fon9/PkCont.hpp"
#include "fon9/TestTools.hpp"
#include "fon9/Endian.hpp"
#include "fon9/CountDownLatch.hpp"

struct Feeder : public fon9::PkContFeeder {
   fon9_NON_COPY_NON_MOVE(Feeder);
   using base = fon9::PkContFeeder;
   Feeder() {
   }
   using base::NextSeq_;
   using base::ReceivedCount_;
   using base::DroppedCount_;
   using base::WaitInterval_;
   SeqT  ExpectedNextSeq_{0};
   SeqT  ExpectedSeq_{0};

   void Feed(SeqT seq) {
      SeqT pk;
      fon9::PutBigEndian(&pk, seq);
      this->FeedPacket(&pk, sizeof(pk), seq);
   }
   void PkContOnReceived(const void* pk, unsigned pksz, SeqT seq) override {
      if (this->ExpectedSeq_ != seq) {
         std::cout << "|err=Unexpected seq=" << seq << "|expected=" << this->ExpectedSeq_
            << "\r[ERROR]" << std::endl;
         abort();
      }
      if (pksz != sizeof(SeqT) || fon9::GetBigEndian<SeqT>(pk) != seq) {
         std::cout << "|err=Invalid pk contents." "\r[ERROR]" << std::endl;
         abort();
      }
      if (this->ExpectedNextSeq_ != this->NextSeq_) {
         std::cout << "|err=Unexpected NextSeq=" << this->NextSeq_
            << "|ExpectedNextSeq=" << this->ExpectedNextSeq_
            << "\r[ERROR]" << std::endl;
         abort();
      }
      this->ExpectedNextSeq_ = seq + 1;
      ++this->ExpectedSeq_;
      if (this->NextSeq_ != seq)
         std::cout << "|gap=[" << this->NextSeq_ << ".." << seq << ")";
   }
   void CheckReceivedCount(SeqT expected) {
      std::cout << "|ReceivedCount=" << this->ReceivedCount_;
      if (this->ReceivedCount_ != expected) {
         std::cout << "|err=ReceivedCount not expected=" << expected
            << "\r[ERROR]" << std::endl;
         abort();
      }
      std::cout << "\r[OK   ]" << std::endl;
   }
};

int main(int argc, char* argv[]) {
   (void)argc; (void)argv;
   fon9::AutoPrintTestInfo utinfo{"PkCont"};
   fon9::GetDefaultTimerThread();
   struct TimeWaiter : public fon9::TimerEntry {
      fon9_NON_COPY_NON_MOVE(TimeWaiter);
      using base = fon9::TimerEntry;
      fon9::CountDownLatch Waiter_{0};
      TimeWaiter() : base{fon9::GetDefaultTimerThread()} {
      }
      virtual void OnTimerEntryReleased() override {
         // TimeWaiter 作為 local data variable, 不需要 delete, 所以 OnTimerEntryReleased(): do nothing.
      }
      virtual void EmitOnTimer(fon9::TimeStamp now) override {
         (void)now;
         this->Waiter_.ForceWakeUp();
      }
      void WaitFor(fon9::TimeInterval ti) {
         this->Waiter_.AddCounter(1);
         this->RunAfter(ti);
         this->Waiter_.Wait();
      }
   };
   TimeWaiter waiter;
   waiter.WaitFor(fon9::TimeInterval{});

   const Feeder::SeqT   kSeqFrom = 123;
   const Feeder::SeqT   kSeqTo = 200;
   Feeder         feeder;
   Feeder::SeqT   seq = kSeqFrom;
   Feeder::SeqT   pkcount = kSeqTo - kSeqFrom + 1;
   feeder.ExpectedSeq_ = seq;
   // 測試1: ExpectedNextSeq==0, 序號=kSeqFrom..kSeqTo
   std::cout << "[TEST ] PkCont|from=" << kSeqFrom << "|to=" << kSeqTo;
   for (; seq <= kSeqTo; ++seq)
      feeder.Feed(seq);
   feeder.CheckReceivedCount(pkcount);

   const Feeder::SeqT   kGapCount = 5;
   seq += kGapCount;
   std::cout << "[TEST ] PkCont.gap and fill|gap=" << kGapCount
      << "|seq=" << seq << ".." << (seq - kGapCount);
   for (Feeder::SeqT L = 0; L <= kGapCount; ++L)
      feeder.Feed(seq - L);
   feeder.CheckReceivedCount(pkcount += kGapCount + 1);

   seq += kGapCount;
   feeder.ExpectedSeq_ = seq;
   std::cout << "[TEST ] PkCont.gap and wait|gap=" << kGapCount
      << "|seq=" << seq << ".." << (seq + kGapCount);
   for (Feeder::SeqT L = 0; L <= kGapCount; ++L)
      feeder.Feed(seq + L);
   waiter.WaitFor(feeder.WaitInterval_ + fon9::TimeInterval_Millisecond(1));
   feeder.CheckReceivedCount(pkcount += kGapCount + 1);
}
