/// \file fon9/Outcome.cpp
/// \author fonwinz@gmail.com
#include "fon9/Outcome.hpp"

namespace fon9 {

[[noreturn]] fon9_API void RaiseOutcomeException(OutcomeSt st) {
   switch (st) {
   default:
   case OutcomeSt::NoValue: Raise<OutcomeNoValue>("Outcome NoValue");
   case OutcomeSt::Result:  Raise<OutcomeHasResult>("Outcome HasResult");
   case OutcomeSt::Error:   Raise<OutcomeHasError>("Outcome IsError");
   }
}

} // namespace
