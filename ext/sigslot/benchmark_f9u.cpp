#include "benchmark_f9u.hpp"

const char* f9u::LibraryName = "fon9 Subr(unsafe)";
const char* f9u::Remark = "fon9::SortedVector";

NOINLINE(void f9u::validate_assert(std::size_t N))
{
    return Benchmark<Signal, f9u>::validation_assert(N);
}    
NOINLINE(double f9u::construction(std::size_t N))
{
    return Benchmark<Signal, f9u>::construction(N);
}
NOINLINE(double f9u::destruction(std::size_t N))
{
    return Benchmark<Signal, f9u>::destruction(N);
}
NOINLINE(double f9u::connection(std::size_t N))
{
    return Benchmark<Signal, f9u>::connection(N);
}
NOINLINE(double f9u::emission(std::size_t N))
{
    return Benchmark<Signal, f9u>::emission(N);
}
NOINLINE(double f9u::combined(std::size_t N))
{
    return Benchmark<Signal, f9u>::combined(N);
}
NOINLINE(double f9u::threaded(std::size_t N))
{
   return 0.0;
}
