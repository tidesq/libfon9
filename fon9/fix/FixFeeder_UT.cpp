// \file fon9/fix/FixFeeder_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "fon9/TestTools.hpp"
#include "fon9/fix/FixFeeder.hpp"
#include "fon9/fix/FixBuilder.hpp"
#include "fon9/fix/FixCompID.hpp"
#include "fon9/Timer.hpp"

namespace f9fix = fon9::fix;

//--------------------------------------------------------------------------//
void BuildTestMessage(f9fix::FixBuilder& fixb, fon9::StrView headerCompIds, unsigned seqNum) {
   #define f9fix_kMSGTYPE_NewOrderSingle  "D"
   fon9::RevPrint(fixb.GetBuffer(), f9fix_SPLTAGEQ(Text), "FixFeederTest #", seqNum);
   fon9::RevPut_TimeFIXMS(fixb.GetBuffer(), fon9::UtcNow()); // SendingTime.
   fon9::RevPrint(fixb.GetBuffer(), f9fix_SPLTAGEQ(SendingTime));
   fon9::RevPrint(fixb.GetBuffer(), f9fix_SPLFLDMSGTYPE(NewOrderSingle) f9fix_SPLTAGEQ(MsgSeqNum), seqNum, headerCompIds);
}
void TestFixFeeder() {
   f9fix::CompIDs compIds{"SenderCoId", "SenderSubId", "TargetCoId", "TargetSubId"};
   std::string fixStream;
   for (unsigned L = 0; L < 500;) {
      ++L;
      f9fix::FixBuilder fixb;
      BuildTestMessage(fixb, fon9::ToStrView(compIds.Header_), L);
      fon9::BufferAppendTo(fixb.Final(f9fix_BEGIN_HEADER_V42), fixStream);
   }

   std::cout << "[TEST ] FixFeeder.";
   struct FixFeeder : public f9fix::FixFeeder {
      fon9_NON_COPY_NON_MOVE(FixFeeder);
      size_t ParsedSize_{0};
      FixFeeder() = default;
      void OnFixMessageParsed(fon9::StrView fixmsg) override {
         this->ParsedSize_ += fixmsg.size();
      }
   };
   FixFeeder  fixFeeder;
   const size_t totsz = fixStream.size();
   const char*  pStreamEnd = fixStream.c_str() + totsz;
   size_t       pers = 0;
   for (size_t feedsz = 1; feedsz <= totsz; ++feedsz) {
      const char* pbeg = fixStream.c_str();
      fixFeeder.ParsedSize_ = 0;
      for (;;) {
         const char* pend = pbeg + feedsz;
         if (pend > pStreamEnd)
            pend = pStreamEnd;
         fon9::DcQueueFixedMem  rxbuf{pbeg, pend};
         auto res = fixFeeder.FeedBuffer(rxbuf);
         if (res < f9fix::FixParser::NeedsMore) {
            std::cout << "FeedBuffer()|err=" << res << "\r[ERROR]" << std::endl;
            abort();
         }
         if (pend >= pStreamEnd) {
            if (res <= f9fix::FixParser::NeedsMore) {
               std::cout << "last FeedBuffer()|err=" << res << "\r[ERROR]" << std::endl;
               abort();
            }
            break;
         }
         pbeg = pend;
      }
      if (fixFeeder.ParsedSize_ != totsz) {
         std::cout << "|ParsedSize=" << fixFeeder.ParsedSize_ << "|expected=" << totsz
            << "\r[ERROR]" << std::endl;
         abort();
      }
      size_t p = feedsz * 100 / totsz;
      if (pers != p) {
         pers = p;
         fprintf(stdout, "%3u%%\b\b\b\b", static_cast<unsigned>(pers));
         fflush(stdout);
      }
   }
   std::cout << "\r" "[OK   ]" << std::endl;
}
//--------------------------------------------------------------------------//

int main() {
#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

   fon9::AutoPrintTestInfo utinfo{"FixFeeder"};
   fon9::GetDefaultTimerThread();
   std::this_thread::sleep_for(std::chrono::milliseconds{10});

   TestFixFeeder();
}
