#ifndef BENCHMARK_f9u_HPP
#define BENCHMARK_f9u_HPP

#include "fon9/Subr.hpp"
#include "../external/signal-slot-benchmarks/benchmark.hpp"

class f9u
{
public:
   NOINLINE(void operator()(Rng& rng))
   {
      volatile std::size_t a = rng(); (void)a;
   }
   using SubrT = fon9::ObjCallback<f9u>;
   struct Signal : public fon9::UnsafeSubject<SubrT> {
      Signal() :fon9::UnsafeSubject<SubrT>(64) {
      }
   };
   ~f9u() {
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

   // This may not be implemented, because: fon9::UnsafeSubject<>
   static double threaded(std::size_t);

   static const char* LibraryName;
   static const char* Remark;

private:
   Signal*        Subject_ = nullptr;
   fon9::SubConn  Connection_;
};

#endif // BENCHMARK_f9u_HPP
