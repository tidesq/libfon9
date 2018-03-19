/// \file fon9/RevFormat.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_RevFormat_hpp__
#define __fon9_RevFormat_hpp__
#include "fon9/RevPrint.hpp"
#include <tuple>
#include <array>

namespace fon9 {

struct FmtArgContextBase {
   virtual void RevPut(RevBuffer& rbuf) const = 0;
   virtual void RevPut(RevBuffer& rbuf, StrView fmt) const = 0;
};

fon9_WARN_DISABLE_PADDING;
fon9_MSC_WARN_DISABLE_NO_PUSH(4265);// C4265: 'fon9::FmtArgContext<std::basic_string<char,std::char_traits<char>,std::allocator<char>>>': class has virtual functions, but destructor is not virtual
template <typename ArgT>
struct FmtArgContext : public FmtArgContextBase {
   ArgT  Value_;

   FmtArgContext(ArgT&& v) : Value_{v} {
   }
   FmtArgContext& operator=(const FmtArgContext&) = delete;

   virtual void RevPut(RevBuffer& rbuf) const override {
      RevPrint(rbuf, this->Value_);
   }
   virtual void RevPut(RevBuffer& rbuf, StrView fmt) const override {
      RevPrint(rbuf, this->Value_, AutoFmt<ArgT>{fmt});
   }
};
fon9_WARN_POP;

template <class... ArgsT>
struct FmtArgStore {
   constexpr FmtArgStore(ArgsT&&... args)
      : Tuple_{std::forward<ArgsT>(args)...}
      , Array_{Tuple_, make_index_sequence<size()>()} {
   }

   static constexpr size_t size() {
      return sizeof...(ArgsT);
   }

   constexpr const FmtArgContextBase*const* data() const {
      return &this->Array_.Data_[0];
   }
private:
   using Tuple = std::tuple<FmtArgContext<ArgsT>...>;
   Tuple Tuple_;

   struct Array {
      std::array<const FmtArgContextBase*, sizeof...(ArgsT)> Data_;

      template<size_t... I>
      constexpr Array(const Tuple& tuple, index_sequence<I...>) : Data_{&std::get<I>(tuple)...} {
      }
   };
   Array Array_;
};

//--------------------------------------------------------------------------//

struct FmtContext {
   RevBuffer*                       Buffer_;
   const FmtArgContextBase*const*   ArgContexts_;
   size_t                           ArgCount_;
   const char*                      FmtEnd_;
};

fon9_API void ParseFmt(const FmtContext& ctx, const char* pfmtcur);

template <class... ArgsT>
inline void RevFormat(RevBuffer& rbuf, const StrView fmtstr, ArgsT&&... args) {
   using ArgStore = FmtArgStore<ArgsT...>;
   ArgStore    argStore{std::forward<ArgsT>(args)...};
   FmtContext  ctx{&rbuf, argStore.data(), argStore.size(), fmtstr.end()};
   ParseFmt(ctx, fmtstr.begin());
}

inline void RevFormat(RevBuffer& rbuf, StrView fmtstr) {
   return RevPutMem(rbuf, fmtstr.begin(), fmtstr.end());
}

//--------------------------------------------------------------------------//

class Fmt : public StopRevPrint {
   StrView FmtStr_;
public:
   explicit Fmt(StrView fmt) : FmtStr_{fmt} {
   }
   template <class... ArgsT>
   void operator()(RevBuffer& rbuf, ArgsT&&... args) {
      RevFormat(rbuf, this->FmtStr_, std::forward<ArgsT>(args)...);
   }
};

template <class... ArgsT>
inline void RevPrint(RevBuffer& rbuf, Fmt fmt, ArgsT&&... args) {
   fmt(rbuf, std::forward<ArgsT>(args)...);
}

} // namespace
#endif//__fon9_RevFormat_hpp__
