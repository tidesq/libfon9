// \file fon9/buffer/RevBuffer_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "fon9/TestTools.hpp"
#include "fon9/buffer/DcQueueList.hpp"
#include "fon9/Log.hpp"
#include "fon9/ThreadId.hpp"

//--------------------------------------------------------------------------//

fon9::RevBufferList InitTestData() {
   fon9::RevBufferList  rbuf{16};
   for (unsigned L = 1; L < 100; ++L)
      fon9::RevPutFill(rbuf, L, static_cast<char>((L % 10) + '0'));
   return rbuf;
}

std::string TestRead(fon9::DcQueue&& dcQu, const size_t sz, const size_t step) {
   std::string res;
   res.resize(sz);
   size_t      pos = 0;
   const char* errmsg;
   for (;;) {
      pos += dcQu.Read(&*res.begin() + pos, step);
      if (pos == sz) {
         if (!dcQu.empty()) {
            errmsg = "|DcQueue remain unused data!";
            break;
         }
         return res;
      }
      if (pos > sz) {
         errmsg = "|DcQueue.Read() overflow!";
         break;
      }
   }
   std::cout << "|step=" << step << errmsg << "\r[ERROR]" << std::endl;
   abort();
}
std::string TestFetch(fon9::DcQueue&& dcQu, const size_t sz, const size_t step) {
   std::string res;
   res.resize(sz);
   size_t      pos = 0;
   const char* errmsg;
   for (;;) {
      char* rdptr = &*res.begin() + pos;
      pos += step;
      if (dcQu.Fetch(rdptr, step) != step) {
         if (pos <= sz) {
            errmsg = "|DcQueue.Fach() overflow!";
            break;
         }
         size_t lastSize = step - (pos - sz);
         pos = sz;
         if (dcQu.Fetch(rdptr, lastSize) != lastSize) {
            errmsg = "|DcQueue.Fach() Last overflow!";
            break;
         }
      }
      if (pos == sz) {
         if (!dcQu.empty()) {
            errmsg = "|DcQueue remain unused data!";
            break;
         }
         return res;
      }
   }
   std::cout << "|step=" << step << errmsg << "\r[ERROR]" << std::endl;
   abort();
}
void TestBuffer() {
   fon9::BufferList  buf{InitTestData().MoveOut()};
   std::string       msg = fon9::BufferTo<std::string>(buf);
   const size_t      msgsz = msg.size();
   std::cout << "TestData|size=" << msgsz << "|nodeCount=" << buf.size() << std::endl;

   std::string res;
   size_t      step;
   std::cout << "[TEST ] DcQueue.Read()";
   for (step = 1; step < msgsz + 10; ++step) {
      res = TestRead(fon9::DcQueueList{InitTestData().MoveOut()}, msgsz, step);
      if (res != msg)
         goto __TEST_ERROR;
   }
   std::cout << "\r[OK   ]" << std::endl;

   std::cout << "[TEST ] DcQueue.Fetch()";
   for (step = 1; step < msgsz + 10; ++step) {
      res = TestFetch(fon9::DcQueueList{InitTestData().MoveOut()}, msgsz, step);
      if (res != msg)
         goto __TEST_ERROR;
   }
   std::cout << "\r[OK   ]" << std::endl;
   return;

__TEST_ERROR:
   std::cout << "|step=" << step << "|result not match!"
      << "\r[ERROR]"
      << "\n|msg=" << msg
      << "\n|res=" << res
      << std::endl;
   abort();
}

//--------------------------------------------------------------------------//

int main() {
   fon9::AutoPrintTestInfo utinfo{"Buffer"};

   // 測試是否可以正確 compile.
   fon9::Decimal<int, 3> dec{123.456};
   fon9_LOG_TRACE("|trace|sizeof(BufferNode)=", sizeof(fon9::BufferNode));
   fon9_LOG_TRACE("|trace|sizeof(RevBufferNode)=", sizeof(fon9::RevBufferNode));
   fon9_LOG_TRACE("|trace|value=", dec, "|sizeof(BufferNodeVirtual)=", sizeof(fon9::BufferNodeVirtual));
   fon9_LOG_DEBUG("|debug|dec+FmtDef=", dec, fon9::FmtDef{});
   fon9_LOG_INFO ("|info |TS=", fon9::UtcNow());
   fon9_LOG_WARN ("|warn |FmtTS=", fon9::UtcNow(), fon9::FmtTS{});
   fon9_LOG_ERROR("|error|value=", dec);
   fon9_LOG_FATAL("|fatal|dec+FmtDef=", dec, fon9::FmtDef{"20.6"});

   utinfo.PrintSplitter();
   const unsigned    kTimes = 1000 * 1000 * 10;
   fon9::StopWatch   stopWatch;

   fon9::RevBufferList  rbuf{1024};
   stopWatch.ResetTimer();
   for (unsigned L = 0; L < kTimes; ++L)
      fon9::RevPrint(rbuf, L);
   {
      fon9::BufferList bufres{rbuf.MoveOut()};
      stopWatch.PrintResultNoEOL("RevPrint(RevBufferList)", kTimes)
         << "|size=" << fon9::CalcDataSize(bufres.cfront())
         << "|nodeCount=" << bufres.size() << std::endl;
   }  // free bufres.

   fon9::NumOutBuf   nbuf;
   size_t            outSize = 0;
   stopWatch.ResetTimer();
   for (unsigned L = 0; L < kTimes; ++L)
      outSize += nbuf.GetLength(fon9::ToStrRev(nbuf.end(), L));
   stopWatch.PrintResultNoEOL("ToStrRev(pout)         ", kTimes)
      << "|size=" << outSize << std::endl;

   utinfo.PrintSplitter();
   TestBuffer();
}
