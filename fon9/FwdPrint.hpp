/// \file fon9/FwdPrint.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_FwdPrint_hpp__
#define __fon9_FwdPrint_hpp__
#include "fon9/RevPut.hpp"
#include "fon9/buffer/FwdBufferList.hpp"

namespace fon9 {

template <class FwdBuffer>
inline void FwdPutChar(FwdBuffer& fbuf, char ch) {
   auto pout = fbuf.AllocSuffix(1);
   *pout = ch;
   fbuf.SetSuffixUsed(pout + 1);
}

template <class FwdBuffer>
inline void FwdPutMem(FwdBuffer& fbuf, const void* mem, size_t sz) {
   auto pout = fbuf.AllocSuffix(sz);
   memcpy(pout, mem, sz);
   fbuf.SetSuffixUsed(pout + sz);
}
template <class FwdBuffer>
inline void FwdPutMem(FwdBuffer& fbuf, const void* pbeg, const void* pend) {
   FwdPutMem(fbuf, pbeg, static_cast<size_t>(reinterpret_cast<const char*>(pend) - reinterpret_cast<const char*>(pbeg)));
}

template <class FwdBuffer>
inline void FwdPrint(FwdBuffer& fbuf, StrView str) {
   FwdPutMem(fbuf, str.begin(), str.size());
}

template <class FwdBuffer, size_t arysz> inline void FwdPrint(FwdBuffer& fbuf, const char (&chary)[arysz])  { return FwdPrint(fbuf, StrView{chary}); }
template <class FwdBuffer, size_t arysz> inline void FwdPrint(FwdBuffer& fbuf, char (&cstr)[arysz])         { return FwdPrint(fbuf, StrView_eos_or_all(cstr)); }
template <class FwdBuffer>               inline void FwdPrint(FwdBuffer& fbuf, char ch)                     { return FwdPutChar(fbuf, ch); }

template <class FwdBuffer, class ValueT>
inline auto FwdPrint(FwdBuffer& fbuf, ValueT&& value) -> decltype(FwdPrint(fbuf, ToStrView(value))) {
   return FwdPrint(fbuf, ToStrView(value));
}

//--------------------------------------------------------------------------//

/// \ingroup AlNum
/// 利用 ToStrRev(pout, value) 把 value 轉成字串填入 fbuf.
template <class FwdBuffer, class ValueT>
inline auto FwdPrint(FwdBuffer& fbuf, ValueT value) -> decltype(ToStrRev(static_cast<char*>(nullptr), value), void()) {
   char   pbuf[ToStrMaxWidth(value)];
   char*  pout = ToStrRev(pbuf + sizeof(pbuf), value);
   size_t sz = static_cast<size_t>(pbuf + sizeof(pbuf) - pout);
   fbuf.SetSuffixUsed(static_cast<char*>(memcpy(fbuf.AllocSuffix(sz), pout, sz)) + sz);
}

//--------------------------------------------------------------------------//

struct StopFwdPrint {};

template <class FwdBuffer, class T1, class T2>
inline auto FwdPrint(FwdBuffer& fbuf, T1&& value1, T2&& value2)
-> enable_if_t<!std::is_same<decay_t<T1>, AutoFmt<T2>>::value
   && !std::is_base_of<StopFwdPrint, decay_t<T1>>::value> {

   FwdPrint(fbuf, std::forward<T1>(value1));
   FwdPrint(fbuf, std::forward<T2>(value2));
}

template <class FwdBuffer, class T1, class T2, class... ArgsT>
inline auto FwdPrint(FwdBuffer& fbuf, T1&& value1, T2&& value2, ArgsT&&... args)
-> enable_if_t<sizeof...(args) != 0
   && !std::is_base_of<StopFwdPrint, decay_t<T1>>::value
   && !std::is_same<decay_t<T1>, AutoFmt<T2>>::value> {

   FwdPrint(fbuf, std::forward<T1>(value1));
   FwdPrint(fbuf, std::forward<T2>(value2), std::forward<ArgsT>(args)...);
}

} // namespace
#endif//__fon9_FwdPrint_hpp__
