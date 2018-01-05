#include "benchmark_f9s.hpp"

const char* f9s::LibraryName = "* fon9 Subr";
const char* f9s::Remark = "fon9::SortedVector";

NOINLINE(void f9s::validate_assert(std::size_t N))
{
    return Benchmark<Signal, f9s>::validation_assert(N);
}    
NOINLINE(double f9s::construction(std::size_t N))
{
    return Benchmark<Signal, f9s>::construction(N);
}
NOINLINE(double f9s::destruction(std::size_t N))
{
    return Benchmark<Signal, f9s>::destruction(N);
}
NOINLINE(double f9s::connection(std::size_t N))
{
    return Benchmark<Signal, f9s>::connection(N);
}
NOINLINE(double f9s::emission(std::size_t N))
{
    return Benchmark<Signal, f9s>::emission(N);
}
NOINLINE(double f9s::combined(std::size_t N))
{
    return Benchmark<Signal, f9s>::combined(N);
}
NOINLINE(double f9s::threaded(std::size_t N))
{
    return Benchmark<Signal, f9s>::threaded(N);
}
