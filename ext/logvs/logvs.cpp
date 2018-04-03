#include <chrono>
#include <thread>
#include <vector>
#include <cstdio>
#include <algorithm>
#include <cstring>

#include <sys/mman.h> // mlockall()

#include "NanoLog.hpp"
#include "spdlog/spdlog.h"
#include "fon9/LogFile.hpp"
#include "fon9/Log.hpp"
#include "fon9/buffer/MemBlockImpl.hpp"
#include "mal_log/mal_log.hpp"


int sleep_us = 0;
int iterations = 100 * 1000;
using latencies_t = std::vector<uint64_t>;

/* Returns nanoseconds since epoch */
uint64_t timestamp_now()
{
    return std::chrono::high_resolution_clock::now().time_since_epoch() / std::chrono::nanoseconds(1);
}

template < typename Function >
void run_log_benchmark(Function && fnTest, latencies_t& latencies)
{
    fnTest(0, "prepare");
    if (sleep_us == 0)
       latencies.reserve(iterations);
    char const * const benchmark = "benchmark";
    for (int i = 0; i < iterations; ++i)
    {
       if(sleep_us && (i % 100) == 0)
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        uint64_t begin = timestamp_now();
        fnTest(i, benchmark);
        uint64_t end = timestamp_now();
        latencies.push_back(end - begin);
        if (sleep_us)
           std::this_thread::sleep_for(std::chrono::microseconds(sleep_us));
    }
}
void print_latencies(latencies_t& latencies)
{
    std::sort(latencies.begin(), latencies.end());
    uint64_t sum = 0; for (auto l : latencies) { sum += l; }

    const size_t iterations = latencies.size();
    char outbuf[1024];
    int  outlen = sprintf(outbuf, "%'11ld|%'11ld|%'11ld|%'11ld|%'11ld|%'11ld|%'11.0f|"
           , latencies[(size_t)iterations * 0.5]
           , latencies[(size_t)iterations * 0.75]
           , latencies[(size_t)iterations * 0.9]
           , latencies[(size_t)iterations * 0.99]
           , latencies[(size_t)iterations * 0.999]
           , latencies[(size_t)iterations * 0.9999]
           , (sum * 1.0) / iterations
        );
    for (size_t i = 3; i >= 1; --i)
        outlen += sprintf(outbuf + outlen, "%'11ld|", latencies[iterations - i]);
    puts(outbuf);
}

template < typename Function >
void run_benchmark(Function && fnTest, int thread_count, char const * const logger)
{
    printf("\nThread count: %d\n", thread_count);
    printf("[%s] percentile latency numbers in nanoseconds\n", logger);
    printf("%11s|%11s|%11s|%11s|%11s|%11s|%11s|%35s|\n"
           , "50th"
           , "75th"
           , "90th"
           , "99th"
           , "99.9th"
           , "99.99th"
           , "Average"
           , "...Worst");
    std::vector<std::thread> threads;
    std::vector<latencies_t> results(thread_count);
    for (int i = 0; i < thread_count; ++i)
    {
        threads.emplace_back(run_log_benchmark<Function>, std::ref(fnTest), std::ref(results[i]));
    }

    latencies_t tot;
    tot.reserve(thread_count * iterations);
    for (int i = 0; i < thread_count; ++i)
    {
        threads[i].join();
        latencies_t& r = results[i];
        print_latencies(r);
        tot.insert(tot.end(), r.begin(), r.end());
    }
    printf("merged:%d\n", static_cast<int>(tot.size()));
    print_latencies(tot);
}

std::vector<int> thrCounts;
template < typename Function >
void run_benchmark_threads(Function && f, char const * const logger)
{
    for (auto threads : thrCounts)
        run_benchmark(f, threads, logger);
}

void print_usage()
{
    printf("Usage: logvs logName iCOUNT sSLEEP t1 t2 t3...\n"
           "    logName     fon9     use fon9_LOG_INFO(test_values)\n"
           "                fon9s    use fon9_LOG_INFO(one_string)\n"
           "                fon9fmt  use fon9_LOG_INFO(fon9::Fmt{}, test_values)\n"
           "                spdlog\n"
           "                nanolog\n"
           "                mal\n"
           "\n"
           "    COUNT       iterations per thread\n"
           "\n"
           "    SLEEP       sleep us after per log\n");
}

int main(int argc, char * argv[])
{
    if (argc < 2)
    {
        print_usage();
        return 0;
    }
    for(int L = 2;  L < argc;  ++L) {
      switch(*argv[L]) {
      case 's': sleep_us = atoi(argv[L]+1); continue;
      case 'i': iterations = atoi(argv[L]+1); continue;
      }
      if (int c = atoi(argv[L])) {
         thrCounts.push_back(c);
      }
    }
    if (thrCounts.empty())
      thrCounts.push_back(1);
    if (iterations <= 0)
       iterations = 100 * 1000;

    setlocale(LC_NUMERIC, "");
    struct lconv *ptrLocale = localeconv();
    ptrLocale->thousands_sep = const_cast<char*>("'");
    printf("\n" "iterations(per thread)=%d|sleep_us(per iterator)=%d", iterations, sleep_us);

    mlockall(MCL_CURRENT|MCL_FUTURE);
    if (strcmp(argv[1], "nanolog") == 0)
    {
        // Guaranteed nano log.
        nanolog::initialize(nanolog::GuaranteedLogger(), "/tmp/", "nanolog", 1024);

        auto nanolog_benchmark = [](int i, char const * const cstr) {  LOG_INFO << "Logging " << cstr << i << 0 << 'K' << -42.42; };
        run_benchmark_threads(nanolog_benchmark, "nanolog_guaranteed");
    }
    else if (strcmp(argv[1], "spdlog") == 0)
    {
        spdlog::set_async_mode(1048576);
        auto spd_logger = spdlog::create < spdlog::sinks::simple_file_sink_mt >("file_logger", "/tmp/spd-async.txt", false);

        auto spdlog_benchmark = [&spd_logger](int i, char const * const cstr) { spd_logger->info("Logging {}{}{}{}{}", cstr, i, 0, 'K', -42.42); };
        run_benchmark_threads(spdlog_benchmark, "spdlog");
    }
    else if(strcmp(argv[1], "fon9") == 0)
    {
        fon9::InitLogWriteToFile("/tmp/fon9-latency.txt", fon9::TimeChecker::TimeScale::No, 0, 0);
        //fon9::impl::MemBlockInit(fon9::kLogBlockNodeSize, 4, 1024); // mem usage: (lists) * (nodes) * 256(bytes)
        auto fon9BenchmarkFn = [](int i, char const * const cstr) { fon9_LOG_INFO("[" __FILE__ ":", __func__, ":" fon9_CTXTOCSTR(__LINE__) "] ", "Logging ", cstr, i, 0, 'K', fon9::Decimal<int64_t, 6>(-42.42), ", for more info."); };
        run_benchmark_threads(fon9BenchmarkFn, "fon9_LOG");
    }
    else if(strcmp(argv[1], "fon9fmt") == 0)
    {
        fon9::InitLogWriteToFile("/tmp/fon9fmt-latency.txt", fon9::TimeChecker::TimeScale::No, 0, 0);
        //fon9::impl::MemBlockInit(fon9::kLogBlockNodeSize, 4, 1024); // mem usage: (lists) * (nodes) * 256(bytes)
        auto fon9BenchmarkFn = [](int i, char const * const cstr) { fon9_LOG_INFO(fon9::Fmt{"Logging {0}{1}{2}{3}{4}"}, cstr, i, 0, 'K', fon9::Decimal<int64_t, 6>(-42.42)); };
        run_benchmark_threads(fon9BenchmarkFn, "fon9_LOG:Fmt{}");
    }
    else if(strcmp(argv[1], "fon9s") == 0)
    {
        fon9::InitLogWriteToFile("/tmp/fon9-str-only.txt", fon9::TimeChecker::TimeScale::No, 0, 0);
        auto fon9BenchmarkFn = [](int i, char const * const cstr) { fon9_LOG_INFO("[" __FILE__ ":" fon9_CTXTOCSTR(__LINE__) "] Logging."); };
        run_benchmark_threads(fon9BenchmarkFn, "fon9_LOG:str-only");
    }
    else if(strcmp(argv[1], "mal") == 0)
    {
        auto m_log = new mal::frontend();
        auto mal_cfg = m_log->get_cfg();
        mal_cfg.file.out_folder          = "/tmp/";
        mal_cfg.file.aprox_size          = 1024 * 1024 * 1024;
        mal_cfg.file.rotation.file_count = 0;
        mal_cfg.file.rotation.delayed_file_count = 0;
        mal_cfg.queue.can_use_heap_q         = true;
        mal_cfg.queue.bounded_q_block_size   = 1048576; 
        mal_cfg.queue.bounded_q_blocking_sev = mal::sev::off;
        m_log->init_backend(mal_cfg);
        auto malBenchmarkFn = [m_log](int i, char const * const cstr) { log_error_i(*m_log, "Logging {}{}{}{c}{}", mal::lit(cstr), i, 0, 'K', -42.42); };
        run_benchmark_threads(malBenchmarkFn, "mal");
        m_log->on_termination();
        delete m_log;
    }
    else
    {
        print_usage();
    }

    return 0;
}

