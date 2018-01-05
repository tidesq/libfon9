#ifndef BENCHMARK_f9s_HPP
#define BENCHMARK_f9s_HPP

#include "fon9/Subr.hpp"
#include "../external/signal-slot-benchmarks/benchmark.hpp"

class f9s
{
public:
   NOINLINE(void operator()(Rng& rng))
   {
      volatile std::size_t a = rng(); (void)a;
   }
   using SubrT = fon9::ObjCallback<f9s>;
   struct Signal : public fon9::Subject<SubrT, std::mutex> {
      Signal() : fon9::Subject<SubrT, std::mutex>(64) {
      }
   };
   ~f9s() {
      if (this->Subject_)
         this->Subject_->Unsubscribe(this->Connection_);
   }

   template <typename Subject, typename Foo>
   static void connect_method(Subject& subject, Foo& foo)
   {
      foo.Subject_ = &subject;
      foo.Connection_ = subject.Subscribe(&foo);
   }
   template <typename Subject>
   static void emit_method(Subject& subject, Rng& rng)
   {
      subject.Publish(rng);
   }

   static void validate_assert(std::size_t);
   static double construction(std::size_t);
   static double destruction(std::size_t);
   static double connection(std::size_t);
   static double emission(std::size_t);
   static double combined(std::size_t);
   static double threaded(std::size_t);

   static const char* LibraryName;
   static const char* Remark;

private:
   Signal*        Subject_ = nullptr;
   fon9::SubConn  Connection_;
};

#endif // BENCHMARK_f9s_HPP
