// \file f9tws/ExgMkt_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "f9tws/ExgMkt.hpp"
#include "fon9/fmkt/SymbIn.hpp"
#include "fon9/RevPrint.hpp"
#include "fon9/TestTools.hpp"

struct Feeder : public f9tws::ExgMktFeeder {
   using base = f9tws::ExgMktFeeder;
   using base::ReceivedCount_;
   using base::ChkSumErrCount_;
   using base::DroppedBytes_;
   uint32_t FmtCount_[f9tws::kExgMktMaxFmtNoSize];
   uint32_t LastSeq_[f9tws::kExgMktMaxFmtNoSize];
   uint32_t SeqMisCount_[f9tws::kExgMktMaxFmtNoSize];

   Feeder() {
      this->ClearStatus();
   }
   void ClearStatus() {
      base::ClearStatus();
      memset(this->FmtCount_, 0, sizeof(this->FmtCount_));
      memset(this->LastSeq_, 0, sizeof(this->LastSeq_));
      memset(this->SeqMisCount_, 0, sizeof(this->SeqMisCount_));
   }
   void ExgMktOnReceived(const f9tws::ExgMktHeader& pk) override {
      auto fmtNo = fon9::PackBcdTo<uint32_t>(pk.FmtNo_);
      ++this->FmtCount_[fmtNo];

      auto seqNo = fon9::PackBcdTo<uint32_t>(pk.SeqNo_);
      if (seqNo != this->LastSeq_[fmtNo] + 1)
         ++this->SeqMisCount_[fmtNo];
      this->LastSeq_[fmtNo] = seqNo;
   }
};
void CheckExgMktFeederStep(const Feeder& orig, const char* buf, size_t bufsz, size_t step) {
   std::cout << "[TEST ] step=" << step << std::flush;
   const char* const     bufend = buf + bufsz;
   const char*           pnext = buf;
   fon9::DcQueueFixedMem dcq{nullptr, nullptr};
   Feeder                feeder;
   while (pnext < bufend) {
      const char* cur = reinterpret_cast<const char*>(dcq.Peek1());
      if (cur == nullptr)
         cur = pnext;
      if ((pnext += step) >= bufend)
         pnext = bufend;
      dcq.Reset(cur, pnext);
      feeder.FeedBuffer(dcq);
   }
   if (feeder.ReceivedCount_ != orig.ReceivedCount_
       || feeder.ChkSumErrCount_ != orig.ChkSumErrCount_
       || feeder.DroppedBytes_ != orig.DroppedBytes_
       || memcmp(feeder.FmtCount_, orig.FmtCount_, sizeof(orig.FmtCount_)) != 0
       || memcmp(feeder.LastSeq_, orig.LastSeq_, sizeof(orig.LastSeq_)) != 0
       || memcmp(feeder.SeqMisCount_, orig.SeqMisCount_, sizeof(orig.SeqMisCount_)) != 0) {
      std::cout << "\r[ERROR]" << std::endl;
      abort();
   }
   std::cout << "\r[OK   ]" << std::endl;
}

struct Fmt6Parser : public Feeder, public fon9::fmkt::SymbTree {
   fon9_NON_COPY_NON_MOVE(Fmt6Parser);
   using baseFeeder = Feeder;
   using baseTree = fon9::fmkt::SymbTree;
   Fmt6Parser() : baseTree{fon9::seed::LayoutSP{}} {
   }

   fon9::fmkt::SymbSP MakeSymb(const fon9::StrView& symbid) override {
      return new fon9::fmkt::SymbIn{symbid};
   }
   void ExgMktOnReceived(const f9tws::ExgMktHeader& pk) override {
      baseFeeder::ExgMktOnReceived(pk);
      auto fmtNo = fon9::PackBcdTo<uint32_t>(pk.FmtNo_);
      if (fmtNo != 6 && fmtNo != 17)
         return;
      const f9tws::ExgMktFmt6v3& fmt6 = *static_cast<const f9tws::ExgMktFmt6v3*>(&pk);
      const f9tws::ExgMktPriQty* pqs  = fmt6.PQs_;
      unsigned                   tmHH = fon9::PackBcdTo<unsigned>(fmt6.Time_.HH_);
      if (fon9_UNLIKELY(tmHH == 99)) // 股票代號"000000"且撮合時間"999999999999",
         return;                     // 表示普通股競價交易末筆即時行情資料已送出.
      uint64_t tmu6 = (((tmHH * 60) + fon9::PackBcdTo<unsigned>(fmt6.Time_.MM_)) * 60
                       + fon9::PackBcdTo<unsigned>(fmt6.Time_.SS_));
      tmu6 = (tmu6 * 1000000) + fon9::PackBcdTo<unsigned>(fmt6.Time_.U6_);
      fon9::StrView        stkno{f9tws::ToStrView(fmt6.StkNo_)};
      SymbMap::Locker      symbs{this->SymbMap_};
      fon9::fmkt::SymbIn&  symb = *static_cast<fon9::fmkt::SymbIn*>(this->FetchSymb(symbs, stkno).get());
      if (fmt6.ItemMask_ & 0x80) {
         symb.Deal_.Data_.Time_.Assign<6>(tmu6);
         symb.Deal_.Data_.TotalQty_ = fon9::PackBcdTo<fon9::fmkt::Qty>(fmt6.TotalQty_);
         pqs->AssignTo(symb.Deal_.Data_.Deal_);
         ++pqs;
      }
      if ((fmt6.ItemMask_ & 0x7e) || (fmt6.ItemMask_ & 1) == 0) {
         symb.BS_.Data_.Time_.Assign<6>(tmu6);
         pqs = AssignBS(symb.BS_.Data_.Buys_, pqs, (fmt6.ItemMask_ & 0x70) >> 4);
         pqs = AssignBS(symb.BS_.Data_.Sells_, pqs, (fmt6.ItemMask_ & 0x0e) >> 1);
      }
   }
   const f9tws::ExgMktPriQty* AssignBS(fon9::fmkt::PriQty* dst, const f9tws::ExgMktPriQty* pqs, int count) {
      if (count > fon9::fmkt::SymbBS::kBSCount)
         count = fon9::fmkt::SymbBS::kBSCount;
      for (int L = 0; L < count; ++L) {
         pqs->AssignTo(*dst);
         ++dst;
         ++pqs;
      }
      if (count < fon9::fmkt::SymbBS::kBSCount)
         memset(dst, 0, sizeof(*dst) * (fon9::fmkt::SymbBS::kBSCount - count));
      return pqs;
   }
};

unsigned long StrToVal(char* str) {
   unsigned long val = strtoul(str, &str, 10);
   switch (toupper(*str)) {
   case 'K': val *= 1024;  break;
   case 'M': val *= 1024 * 1024;  break;
   case 'G': val *= 1024 * 1024 * 1024;  break;
   }
   return val;
}

int main(int argc, char* argv[]) {
   fon9::AutoPrintTestInfo utinfo{"f9tws ExgMkt"};

   if (argc < 4) {
      std::cout << "Usage: TwsExgMktFileName ReadFrom ReadSize [Steps]\n"
         "ReadSize=0 for read to EOF.\n"
         "Steps=0 for test Fmt6+17 parsing.\n"
         << std::endl;
      return 3;
   }
   std::cout << "Mkt File=" << argv[1] << std::endl;
   FILE* fd = fopen(argv[1], "rb");
   if (!fd) {
      perror("Open MktFile fail");
      return 3;
   }
   fseek(fd, 0, SEEK_END);
   auto fsize = ftell(fd);
   std::cout << "FileSize=" << fsize << std::endl;

   size_t rdfrom = StrToVal(argv[2]);
   std::cout << "ReadFrom=" << rdfrom << std::endl;
   if (rdfrom >= static_cast<size_t>(fsize)) {
      std::cout << "ReadFrom >= FileSize" << std::endl;
      return 3;
   }
   size_t rdsz = StrToVal(argv[3]);
   if (rdsz <= 0)
      rdsz = fsize - rdfrom;
   else if (rdfrom + rdsz > static_cast<size_t>(fsize)) {
      std::cout << "ReadFrom + ReadSize > FileSize" << std::endl;
      return 3;
   }
   std::cout << "ReadSize=" << rdsz << std::endl;
   char* const rdbuf = static_cast<char*>(malloc(rdsz));
   if (rdbuf == nullptr) {
      std::cout << "err=Alloc buffer for read." << std::endl;
      return 3;
   }
   fseek(fd, static_cast<long>(rdfrom), SEEK_SET);
   size_t r = fread(rdbuf, 1, rdsz, fd);
   if (r != rdsz) {
      free(rdbuf);
      std::cout << "err=Read " << r << " bytes, but expect=" << rdsz << std::endl;
      return 3;
   }
   fclose(fd);

   Feeder feeder;
   fon9::DcQueueFixedMem dcq{rdbuf, rdsz};

   fon9::StopWatch stopWatch;
   feeder.FeedBuffer(dcq);
   const double tmFull = stopWatch.StopTimer();
   stopWatch.PrintResult(tmFull, "Fetch packet", feeder.ReceivedCount_);

   std::cout << "ReceivedCount=" << feeder.ReceivedCount_
      << "|ChkSumErrCount=" << feeder.ChkSumErrCount_
      << "|DroppedBytes=" << feeder.DroppedBytes_
      << std::endl;

   for (unsigned L = 0; L < f9tws::kExgMktMaxFmtNoSize; ++L) {
      if (feeder.FmtCount_[L]) {
         std::cout << "FmtNo=" << L
            << "|Count=" << feeder.FmtCount_[L]
            << "|LastSeq=" << feeder.LastSeq_[L]
            << "|MisCount=" << feeder.SeqMisCount_[L]
            << std::endl;
      }
   }

   if (argc >= 5) {
      auto step = StrToVal(argv[4]);
      if (step > 0) {
         stopWatch.ResetTimer();
         CheckExgMktFeederStep(feeder, rdbuf, rdsz, step);
         stopWatch.PrintResult("Fetch packet by step", feeder.ReceivedCount_);
      }
      else { // 解析 Fmt6 => SymbIn: 實際應用時, 還有序號連續性問題, 所以要花更久的時間.
         Fmt6Parser fmt6parser;
         dcq.Reset(rdbuf, rdbuf + rdsz);
         stopWatch.ResetTimer();
         fmt6parser.FeedBuffer(dcq);
         const double tmFmt6 = stopWatch.StopTimer();
         stopWatch.PrintResult(tmFmt6 - tmFull, "Parse Fmt6+17", feeder.FmtCount_[6] + feeder.FmtCount_[17]);
         // 測試結果:
         // Hardware: HP ProLiant DL380p Gen8 / E5-2680 v2 @ 2.80GHz
         // OS: Ubuntu 16.04.2 LTS
         // Parse Fmt6+17: 2.673751904 secs / 17,034,879 times = 156.957493153 ns

         std::cout << "Symbol count=" << fmt6parser.SymbMap_.Lock()->size() << std::endl;
         auto symb = fmt6parser.GetSymb("2330");
         if (!symb && !fmt6parser.SymbMap_.Lock()->empty())
            symb.reset(&fon9::fmkt::GetSymbValue(*fmt6parser.SymbMap_.Lock()->begin()));
         if (const fon9::fmkt::SymbIn* symi = static_cast<const fon9::fmkt::SymbIn*>(symb.get())) {
            fon9::RevBufferList rbuf{256};
            fon9::FmtDef fmtPri{7,2};
            fon9::FmtDef fmtQty{7};
            for (int L = fon9::fmkt::SymbBS::kBSCount; L > 0;) {
               --L;
               RevPrint(rbuf, symi->BS_.Data_.Buys_[L].Pri_, fmtPri, " / ", symi->BS_.Data_.Buys_[L].Qty_, fmtQty, '\n');
            }
            RevPrint(rbuf, "---\n");
            for (int L = 0; L < fon9::fmkt::SymbBS::kBSCount; ++L) {
               RevPrint(rbuf, symi->BS_.Data_.Sells_[L].Pri_, fmtPri, " / ", symi->BS_.Data_.Sells_[L].Qty_, fmtQty, '\n');
            }
            fon9::RevPrint(rbuf,
               "SymbId=", symi->SymbId_, "\n"
               "Last deal: ", *static_cast<const fon9::TimeInterval*>(&symi->Deal_.Data_.Time_), "\n"
               "      Pri: ", symi->Deal_.Data_.Deal_.Pri_, fmtPri, "\n"
               "      Qty: ", symi->Deal_.Data_.Deal_.Qty_, fmtQty, "\n"
               " TotalQty: ", symi->Deal_.Data_.TotalQty_, fmtQty, "\n"
               "Last Sells - Buys: ", *static_cast<const fon9::TimeInterval*>(&symi->BS_.Data_.Time_), "\n"
               );
            std::cout << fon9::BufferTo<std::string>(rbuf.MoveOut()) << std::endl;
         }
      }
   }

   free(rdbuf);
}
