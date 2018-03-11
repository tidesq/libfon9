/// \file fon9/Exception.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_Exception_hpp__
#define __fon9_Exception_hpp__
#include "fon9/sys/Config.hpp"
#include <stdexcept>

namespace fon9 {

/// \ingroup Misc
/// 丟出異常.
/// 透過一層間接呼叫, 可讓 compiler 更容易最佳化: 會丟出異常的 function.
template <class E, class... ArgsT>
[[noreturn]] void Raise(ArgsT&&... args) {
   throw E{std::forward<ArgsT>(args)...};
}
/// \ingroup Misc
/// 丟出異常.
/// - 指定返回值型別, 雖然Raise()不會返回。
/// - 就可以使用 `cond ? Raise<ReturnT,ex>(msg) : retval;` 這樣的敘述。
/// - 這在 constexpr function 裡面很常用到。
template <class ReturnT, class E, class... ArgsT>
[[noreturn]] ReturnT Raise(ArgsT&&... args) {
   throw E{std::forward<ArgsT>(args)...};
}

fon9_MSC_WARN_DISABLE(4623); // 4623: 'fon9::BufferOverflow': default constructor was implicitly defined as deleted
#define fon9_DEFINE_EXCEPTION(exceptionName, baseException) \
class exceptionName : public baseException { \
   using base = baseException; \
public: \
   exceptionName() = default; \
   using base::base; \
};

fon9_DEFINE_EXCEPTION(BufferOverflow, std::runtime_error);

fon9_MSC_WARN_POP;

} // namespace
#endif//__fon9_Exception_hpp__
