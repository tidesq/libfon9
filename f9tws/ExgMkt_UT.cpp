// \file f9tws/ExgMkt_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "f9tws/ExgMkt.hpp"
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
      std::cout << "Usage: TwsExgMktFileName ReadFrom ReadSize [step]" << std::endl;
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
   stopWatch.PrintResult("Fetch packet", feeder.ReceivedCount_);

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
   }

   free(rdbuf);
}
