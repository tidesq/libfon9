// \file fon9/LogFile_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "fon9/TestTools.hpp"
#include "fon9/LogFile.hpp"
#include "fon9/Log.hpp"
#include "fon9/RevFormat.hpp"
#include "fon9/ThreadId.hpp"
#include "fon9/ThreadTools.hpp"
#include <vector>
#include <numeric>

unsigned gNumberOfThreads{0};

//--------------------------------------------------------------------------//

void TestAppender(fon9::Appender& app) {
   static const unsigned kAppendTimesPerThread = 1000 * 1000;
   static const unsigned kNodesPerAppend = 4;
   using ThreadPool = std::array<std::thread, 4>;
   ThreadPool  thrs;
   uint64_t    thrId = 0;
   for (std::thread& thr : thrs) {
      ++thrId;
      thr = std::thread{[&app, thrId]() {
         for (unsigned L = 0; L < kAppendTimesPerThread; ++L) {
            fon9::BufferList  buf;
            for (unsigned nodeId = 0; nodeId < kNodesPerAppend; ++nodeId) {
               fon9::RevBufferNode* node = fon9::RevBufferNode::Alloc(10);
               fon9::byte* pout = node->GetDataBegin();
               if (nodeId == kNodesPerAppend - 1)
                  *--pout = '\n';
               memset(pout -= 10, static_cast<char>(thrId + '0'), 10);
               if (nodeId != 0)
                  *--pout = '-';
               node->SetPrefixUsed(pout);
               buf.push_back(node);
            }
            app.Append(std::move(buf));
         }
      }};
   }
   fon9::JoinThreads(thrs);
}
void TestTimerFlush() {
   fon9::RevBufferFixedSize<1024> rbuf;
   fon9::AsyncFileAppenderSP      fapp{fon9::LogFileAppender::Make()};
   auto oresfut = fapp->Open("./logs/fapp.log", fon9::FileMode::CreatePath);
   auto ores = oresfut.get();
   rbuf.RewindEOS();
   fon9::RevPrint(rbuf, "OpenResult|", ores);
   std::cout << rbuf.GetCurrent() << std::endl;

   char rdbuf[1024];
   while (fgets(rdbuf, sizeof(rdbuf), stdin)) {
      fon9::StrView  rd{fon9::StrView_cstr(rdbuf)};
      fapp->Append(rd);
      fon9::StrTrim(rd);
      if (rd == "flush")
         fapp->WaitFlushed();
      else if (rd == "quit")
         break;
   }
}

//--------------------------------------------------------------------------//

static uint64_t   gLogBytes;
static void LogBenchmark(std::string testName) {
   testName.resize(16, ' ');
   const unsigned kTimes = 1000 * 1000;
   const bool     kLongLog = false;
   std::string    empty = " ";
   std::string    longStr(3000, 'X');
   longStr += " ";

   fon9::StopWatch stopWatch;
   for (unsigned L = 0; L < kTimes; ++L)
      fon9_LOG_INFO("Hello 0123456789", " abcdefghijklmnopqrstuvwxyz", (kLongLog ? longStr : empty), L,
                    "|@" __FILE__ ":" fon9_CTXTOCSTR(__LINE__));
   double span1 = stopWatch.StopTimer();
   fon9::WaitLogFileFlushed();
   double span2 = stopWatch.StopTimer();
   stopWatch.PrintResultNoEOL(span1, testName.c_str(), kTimes)
      << std::setprecision(2)
      << "|Mops=" << kTimes / span1 / 1000000
      << "|bytes=" << gLogBytes
      << "|MBps=" << static_cast<double>(gLogBytes) / span1 / (1024 * 1024)
      << "|MBps(flushed)=" << static_cast<double>(gLogBytes) / (span1+span2) / (1024 * 1024)
      << std::setprecision(9)
      << std::endl;
}
static void NopLogWriter(const fon9::LogArgs&, fon9::BufferList&& buf) {
   using Result = fon9::File::Result;
   fon9::DeviceOutputBlock(fon9::DcQueueList{std::move(buf)}, [](const void*, size_t sz)->Result {
      gLogBytes += sz;
      return Result{sz};
   });
}
void BenchLogToFile(std::string fname) {
   if (memcmp(fname.c_str(), "/dev/", 5) != 0)
      remove(fname.c_str());
   fon9::InitLogWriteToFile(fname, fon9::TimeChecker::TimeScale::No, 0, 0);
   LogBenchmark(fname);
}

//--------------------------------------------------------------------------//

void BenchThreadsWrite() {
   // 使用 spdlog-async.cpp 相同測試方法:
   // https://github.com/gabime/spdlog/blob/master/bench/spdlog-async.cpp
   fon9::InitLogWriteToFile("./logs/fon9-bench.log", fon9::TimeChecker::TimeScale::No, 0, 0);
   std::atomic<unsigned>      msgCounter{0};
   std::vector<std::thread>   threads;
   const unsigned             kHowmany = 1000000;
   fon9::StopWatch stopWatch;
   for (unsigned t = 0; t < gNumberOfThreads; ++t) {
      threads.push_back(std::thread([&]() {
         for (;;) {
            unsigned counter = ++msgCounter;
            if (counter > kHowmany)
               break;
            fon9_LOG_INFO(fon9::Fmt{"fon9 log message #{0}: This is some text for your pleasure"}, counter);
         }
      }));
   }
   fon9::JoinThreads(threads);
   double deltaf = stopWatch.StopTimer();
   auto   rate   = kHowmany / deltaf;

   std::cout
      << "Total:   " << kHowmany << " messages\n"
      << "Threads: " << gNumberOfThreads << "\n"
      << "Delta =  " << deltaf << " seconds\n"
      << "Rate =   " << rate << " messages/sec"
      << std::endl;
}

//--------------------------------------------------------------------------//

uint64_t timestamp_now() {
   return static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch() / std::chrono::nanoseconds(1));
}

template < typename Function >
void run_log_benchmark(Function&& fn) {
   const unsigned iterations = 100000;
   std::vector<uint64_t>   latencies;
   latencies.reserve(iterations);

   char const * const   cstrBenchmark = "benchmark";
   for (unsigned i = 0; i < iterations; ++i) {
      uint64_t begin = timestamp_now();
      fn(i, cstrBenchmark);
      uint64_t end = timestamp_now();
      latencies.push_back(end - begin);
   }
   std::sort(latencies.begin(), latencies.end());
   uint64_t sum = 0; for (auto v : latencies) { sum += v; }
   fon9::RevBufferFixedSize<1024> rbuf;
   rbuf.RewindEOS();
   fon9::RevFormat(rbuf, "{0:,11}|{1:,11}|{2:,11}|{3:,11}|{4:,11}|{5:,11}|{6:,11.0}|",
                  latencies[static_cast<size_t>(iterations * 0.5)],
                  latencies[static_cast<size_t>(iterations * 0.75)],
                  latencies[static_cast<size_t>(iterations * 0.9)],
                  latencies[static_cast<size_t>(iterations * 0.99)],
                  latencies[static_cast<size_t>(iterations * 0.999)],
                  latencies[latencies.size() - 1],
                  fon9::Decimal<int64_t, 6>(static_cast<double>(sum) / static_cast<double>(latencies.size())));
   std::cout << rbuf.GetCurrent() << std::endl;
}

template < typename Function >
void run_benchmark(Function && fn, unsigned threadCount, char const * const loggerName) {
   std::cout << "threadCount=" << threadCount << "|loggerName=" << loggerName << "\n"
      "       50th|       75th|       90th|       99th|     99.9th|      Worst|    Average|" << std::endl;

   std::vector<std::thread>   threads;
   for (unsigned i = 0; i < threadCount; ++i)
      threads.emplace_back(run_log_benchmark<Function>, std::ref(fn));
   fon9::JoinThreads(threads);
}

void TestThreadsWriteLatency() {
   // 使用的測試方法: https://github.com/Iyengar111/NanoLog#latency-benchmark-of-guaranteed-logger
   fon9::InitLogWriteToFile("./logs/fon9-latency.log", fon9::TimeChecker::TimeScale::No, 0, 0);
   auto fon9BenchmarkFn = [](unsigned i, char const * const cstr) {
      fon9_LOG_INFO("Logging ", cstr, i, 0, 'K', fon9::Decimal<int64_t, 6>(-42.42));
   };
   for (auto threadCount : {1u, 2u, 4u, 8u})
      run_benchmark(fon9BenchmarkFn, threadCount, "fon9_LOG");
}

//--------------------------------------------------------------------------//

int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
   if (argc >= 2)
      gNumberOfThreads = static_cast<size_t>(atoi(argv[1]));
   if (gNumberOfThreads <= 0) {
      if ((gNumberOfThreads = std::thread::hardware_concurrency()) <= 0)
         gNumberOfThreads = 4;
   }
   if (argc >= 3 && strcmp(argv[2], "lat")==0) {
      const char* latlog = "./logs/fon9-latency.log";
      remove(latlog);
      fon9::InitLogWriteToFile(latlog, fon9::TimeChecker::TimeScale::No, 0, 0);
      auto fon9BenchmarkFn = [](unsigned i, char const * const cstr) {
         fon9_LOG_INFO("Logging ", cstr, i, 0, 'K', fon9::Decimal<int64_t, 6>(-42.42));
      };
      run_benchmark(fon9BenchmarkFn, gNumberOfThreads, "fon9_LOG");
      return 0;
   }

   fon9::AutoPrintTestInfo utinfo{"LogFile"};
   auto res = fon9::InitLogWriteToFile("./logs/Scale_Second_{0:f-t+8}.{1:04}.log", fon9::TimeChecker::TimeScale::Second, 1024, 0);

   fon9::RevBufferFixedSize<1024> rbuf;
   rbuf.RewindEOS();
   fon9::RevPrint(rbuf, "LogOpenResult|", res);
   std::cout << rbuf.GetCurrent() << std::endl;
   if (!res) {
      std::cout << "|Log file open fail!" << std::endl;
      abort();
   }
   for (unsigned L = 0; L < 40; ++L) {
      std::flush(std::cout << "\r" "L=" << L);
      fon9_LOG_TRACE("test:|L=", L, "|abcdefghijklmnopqrstuvwxyz====ABCDEFGHIJKLMNOPQRSTUVWXYZ====123456789-123456789-123456789-123456789-");
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
   }
   std::cout << std::endl;

   // 使用 moduo 的 logging_test 測試方法:
   // https://github.com/chenshuo/muduo/blob/master/muduo/base/tests/Logging_test.cc
   utinfo.PrintSplitter();
   fon9::SetLogWriter(&NopLogWriter, fon9::TimeZoneOffset{});
   LogBenchmark("nop");
#ifndef fon9_WINDOWS
   BenchLogToFile("/dev/null");
   BenchLogToFile("/tmp/fon9.log");
#endif
   BenchLogToFile("./logs/bench.log");

   utinfo.PrintSplitter();
   BenchThreadsWrite();

   utinfo.PrintSplitter();
   TestThreadsWriteLatency();
   return 0;
}
