/// \file fon9/RevFormat.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_RevFormat_hpp__
#define __fon9_RevFormat_hpp__
#include "fon9/RevPrint.hpp"
#include <tuple>
#include <array>
#include <vector>

namespace fon9 {

struct FmtArgContextBase {
   virtual void RevPut(RevBuffer& rbuf) const = 0;
   virtual void RevPut(RevBuffer& rbuf, StrView fmt) const = 0;
};

fon9_WARN_DISABLE_PADDING;
fon9_MSC_WARN_DISABLE_NO_PUSH(4265// C4265: 'fon9::FmtArgContext<std::basic_string<char,std::char_traits<char>,std::allocator<char>>>': class has virtual functions, but destructor is not virtual
                              4251// dll-interface
                              );
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

//--------------------------------------------------------------------------//

class fon9_API FmtPre : public StopRevPrint {
   struct FmtPreSlice {
      /// <0:  this 定義的是一般字串.
      /// >=0: this 定義的是參數 {ArgId_:fmt} 的格式.
      int         ArgId_;
      unsigned    Begin_;
      unsigned    End_;
      unsigned    BrBegin_;// {n:} 左括號的開始位置.
   };
   using Slices = std::vector<FmtPreSlice>;

   std::string FmtStr_;
   Slices      Slices_;
   int         MaxArgId_{-1};
   void AddSlice(int argid, const char* pbeg, const char* pend);
   const char* ParseFmtArg(int argid, const char* pfmtcur, const char* pfmtend);
   void ParseFmt(const char* pfmtcur, const char* pfmtend);
   void DoFormat(const FmtContext& ctx) const;
public:
   FmtPre() = default;

   FmtPre(const FmtPre&) = default;
   FmtPre& operator=(const FmtPre&) = default;

   FmtPre(FmtPre&& rhs) {
      *this = std::move(rhs);
   }
   FmtPre& operator=(FmtPre&& rhs) {
      this->FmtStr_ = std::move(rhs.FmtStr_);
      this->Slices_ = std::move(rhs.Slices_);
      this->MaxArgId_ = rhs.MaxArgId_;
      rhs.MaxArgId_ = -1;
      return *this;
   }

   explicit FmtPre(std::string fmt) {
      this->Parse(std::move(fmt));
   }
   void Parse(std::string fmt);

   /// 取得格式設定裡面最大的參數Id, <0表示沒有任何參數.
   int GetMaxArgId() const {
      return this->MaxArgId_;
   }

   /// 取得第一個出現 argid 的格式設定.
   /// \retval false 沒有此 argid 的輸出.
   /// \retval true
   /// - retval.begin()==nullptr 表示有此參數，但沒有提供格式: {n}
   /// - 此參數，且有提供格式: {n:fmt}，fmt可能為 empty();
   bool GetArgIdFormat(unsigned argid, StrView& fmt) const;

   template <class... ArgsT>
   void operator()(RevBuffer& rbuf, ArgsT&&... args) const {
      using ArgStore = FmtArgStore<ArgsT...>;
      ArgStore    argStore{std::forward<ArgsT>(args)...};
      FmtContext  ctx{&rbuf, argStore.data(), argStore.size(), static_cast<const char*>(nullptr)};
      this->DoFormat(ctx);
   }
};

template <class... ArgsT>
inline void RevPrint(RevBuffer& rbuf, const FmtPre& fmtPre, ArgsT&&... args) {
   fmtPre(rbuf, std::forward<ArgsT>(args)...);
}
fon9_WARN_POP;

} // namespace
#endif//__fon9_RevFormat_hpp__
