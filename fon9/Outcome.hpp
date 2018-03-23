/// \file fon9/Outcome.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_Outcome_hpp__
#define __fon9_Outcome_hpp__
#include "fon9/Exception.hpp"
#include "fon9/ErrC.hpp"
#include "fon9/Utility.hpp"

namespace fon9 {

fon9_MSC_WARN_DISABLE(4623); // 4623: default constructor was implicitly defined as deleted
fon9_DEFINE_EXCEPTION(OutcomeNoValue, std::logic_error);
fon9_DEFINE_EXCEPTION(OutcomeHasResult, std::logic_error);
fon9_DEFINE_EXCEPTION(OutcomeHasError, std::logic_error);
fon9_MSC_WARN_POP;

enum class OutcomeSt {
   /// 無任何資料.
   NoValue = 0,
   /// 沒有錯誤, 使用 ResultType 提供結果.
   Result = 1,
   /// 使用 ErrorType 提供錯誤資訊.
   Error = 2,
};

[[noreturn]] fon9_API void RaiseOutcomeException(OutcomeSt st);

fon9_WARN_DISABLE_PADDING;
fon9_GCC_WARN_DISABLE("-Wstrict-aliasing");
//fon9_MSC_WARN_DISABLE_NO_PUSH(4582  //4582: 'fon9::Outcome<Value,fon9::ErrC>::ResultTemp_': constructor is not implicitly called
//                              4583);//4583: 'fon9::Outcome<Value,fon9::ErrC>::ResultTemp_': destructor is not implicitly called

template <class ResultT, class ErrorT = ErrC>
class Outcome {
   union {
      //使用 char[] 記憶體使用量可能會較小.
      //ResultT  ResultTemp_;
      //ErrorT   ErrorTemp_;
      char     Result_[sizeof(ResultT)];
      char     Error_[sizeof(ErrorT)];
   };
   OutcomeSt   St_;

   ResultT& GetResultImpl() { return *reinterpret_cast<ResultT*>(this->Result_); }
   ErrorT& GetErrorImpl() { return *reinterpret_cast<ErrorT*>(this->Error_); }
   const ResultT& GetResultImpl() const { return *reinterpret_cast<const ResultT*>(this->Result_); }
   const ErrorT& GetErrorImpl() const { return *reinterpret_cast<const ErrorT*>(this->Error_); }

   void InplaceNew(const Outcome& rhs) {
      switch (rhs.St_) {
      case OutcomeSt::NoValue: return;
      case OutcomeSt::Result:  fon9::InplaceNew<ResultT>(&this->Result_, rhs.GetResultImpl()); break;
      case OutcomeSt::Error:   fon9::InplaceNew<ErrorT>(&this->Error_, rhs.GetErrorImpl());    break;
      }
   }
   void InplaceMove(Outcome&& rhs) {
      switch (rhs.St_) {
      case OutcomeSt::NoValue: return;
      case OutcomeSt::Result:  fon9::InplaceNew<ResultT>(&this->Result_, std::move(rhs.GetResultImpl())); break;
      case OutcomeSt::Error:   fon9::InplaceNew<ErrorT>(&this->Error_, std::move(rhs.GetErrorImpl()));    break;
      }
   }

public:
   using ResultType = ResultT;
   using ErrorType = ErrorT;

   ~Outcome() {
      this->Clear();
      static_assert((static_cast<void*>(&static_cast<Outcome*>(nullptr)->Result_) == &static_cast<Outcome*>(nullptr)->Error_),
                    "'&Outcome.Result_' != '&Outcome.Error_'");
   }

   // Outcome() : St_{OutcomeSt::NoValue} {}
   Outcome() = delete;

   explicit Outcome(const ResultT& r) : St_{OutcomeSt::Result} {
      fon9::InplaceNew<ResultT>(&this->Result_, r);
   }
   explicit Outcome(ResultT&& r) : St_{OutcomeSt::Result} {
      fon9::InplaceNew<ResultT>(&this->Result_, std::move(r));
   }

   Outcome(const ErrorT& err) : St_{OutcomeSt::Error} {
      fon9::InplaceNew<ErrorT>(&this->Error_, err);
   }
   Outcome(ErrorT&& err) : St_{OutcomeSt::Error} {
      fon9::InplaceNew<ErrorT>(&this->Error_, std::move(err));
   }

   Outcome(const Outcome& rhs) : St_{rhs.St_} {
      this->InplaceNew(rhs);
   }
   Outcome(Outcome&& rhs) : St_{rhs.St_} {
      this->InplaceMove(std::move(rhs));
   }
   Outcome& operator=(const Outcome& rhs) {
      if (this->St_ == rhs.St_) {
         switch (this->St_) {
         case OutcomeSt::NoValue: break;
         case OutcomeSt::Result:  this->GetResultImpl() = rhs.GetResultImpl(); break;
         case OutcomeSt::Error:   this->GetErrorImpl()  = rhs.GetErrorImpl();  break;
         }
      }
      else {
         this->Clear();
         this->St_ = rhs.St_;
         this->InplaceNew(rhs);
      }
      return *this;
   }
   Outcome& operator=(Outcome&& rhs) {
      if (this->St_ == rhs.St_) {
         switch (this->St_) {
         case OutcomeSt::NoValue: break;
         case OutcomeSt::Result:  this->GetResultImpl() = std::move(rhs.GetResultImpl()); break;
         case OutcomeSt::Error:   this->GetErrorImpl() = std::move(rhs.GetErrorImpl());   break;
         }
      }
      else {
         this->Clear();
         this->St_ = rhs.St_;
         this->InplaceMove(std::move(rhs));
      }
      return *this;
   }

   Outcome& operator=(const ResultT& rhs) {
      if (this->St_ == OutcomeSt::Result)
         this->GetResultImpl() = rhs;
      else {
         this->Clear();
         this->St_ = OutcomeSt::Result;
         fon9::InplaceNew<ResultT>(&this->Result_, rhs);
      }
      return *this;
   }
   Outcome& operator=(ResultT&& rhs) {
      if (this->St_ == OutcomeSt::Result)
         this->GetResultImpl() = std::move(rhs);
      else {
         this->Clear();
         this->St_ = OutcomeSt::Result;
         fon9::InplaceNew<ResultT>(&this->Result_, std::move(rhs));
      }
      return *this;
   }

   Outcome& operator=(const ErrorT& rhs) {
      if (this->St_ == OutcomeSt::Error)
         this->GetErrorImpl() = rhs;
      else {
         this->Clear();
         this->St_ = OutcomeSt::Error;
         fon9::InplaceNew<ErrorT>(&this->Error_, rhs);
      }
      return *this;
   }
   Outcome& operator=(ErrorT&& rhs) {
      if (this->St_ == OutcomeSt::Error)
         this->GetErrorImpl() = std::move(rhs);
      else {
         this->Clear();
         this->St_ = OutcomeSt::Error;
         fon9::InplaceNew<ErrorT>(&this->Error_, std::move(rhs));
      }
      return *this;
   }

   void Clear() {
      switch (this->St_) {
      case OutcomeSt::NoValue: return;
      case OutcomeSt::Result:  destroy_at(&this->GetResultImpl()); break;
      case OutcomeSt::Error:   destroy_at(&this->GetErrorImpl());  break;
      }
      this->St_ = OutcomeSt::NoValue;
   }

   bool operator==(const Outcome& rhs) const {
      if (this->St_ == rhs.St_) {
         switch (this->St_) {
         case OutcomeSt::NoValue: return true;
         case OutcomeSt::Result:  return this->GetResultImpl() == rhs.GetResultImpl(); break;
         case OutcomeSt::Error:   return this->GetErrorImpl() == rhs.GetErrorImpl();   break;
         }
      }
      return false;
   }
   bool operator!=(const Outcome& rhs) const {
      return !this->operator==(rhs);
   }

   /// 沒有結果時(!= OutcomeSt::Result, 可能為 OutcomeSt::NoValue 或 OutcomeSt::Error), 傳回 true.
   constexpr bool operator!() const {
      return this->St_ != OutcomeSt::Result;
   }
   /// 有結果時(== OutcomeSt::Result), 傳回 true.
   explicit operator bool() const {
      return this->St_ == OutcomeSt::Result;
   }

   /// throws: OutcomeNoValue, OutcomeHasError
   ResultT& GetResult() {
      if (this->St_ == OutcomeSt::Result)
         return this->GetResultImpl();
      RaiseOutcomeException(this->St_);
   }
   const ResultT& GetResult() const {
      if (this->St_ == OutcomeSt::Result)
         return this->GetResultImpl();
      RaiseOutcomeException(this->St_);
   }

   /// throws: OutcomeNoValue, OutcomeHasResult
   ErrorT& GetError() {
      if (this->St_ == OutcomeSt::Error)
         return this->GetErrorImpl();
      RaiseOutcomeException(this->St_);
   }
   const ErrorT& GetError() const {
      if (this->St_ == OutcomeSt::Error)
         return this->GetErrorImpl();
      RaiseOutcomeException(this->St_);
   }

   constexpr bool HasResult() const {
      return this->St_ == OutcomeSt::Result;
   }
   constexpr bool IsError() const {
      return this->St_ == OutcomeSt::Error;
   }
   constexpr bool IsNoValue() const {
      return this->St_ == OutcomeSt::NoValue;
   }
   constexpr OutcomeSt GetState() const {
      return this->St_;
   }
};
fon9_WARN_POP;

} // namespace
#endif//__fon9_Outcome_hpp__
