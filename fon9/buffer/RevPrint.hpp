/// \file fon9/buffer/RevPrint.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_buffer_RevPrint_hpp__
#define __fon9_buffer_RevPrint_hpp__
#include "fon9/buffer/RevPut.hpp"
#include "fon9/ToStrFmt.hpp"
#include <algorithm>

namespace fon9 {

template <class RevBuffer>
inline void RevPutStr(RevBuffer& rbuf, StrView str) {
   RevPutMem(rbuf, str.begin(), str.end());
}

template <class RevBufferT>
void RevPutStr(RevBufferT& rbuf, StrView str, FmtDef fmt) {
   char* pout = rbuf.AllocPrefix(std::max(static_cast<size_t>(fmt.Width_), str.size()));
   pout = ToStrRev(pout, str, fmt);
   rbuf.SetPrefixUsed(pout);
}

template <class RevBuffer, size_t arysz> inline void RevPutStr(RevBuffer& rbuf, const char (&chary)[arysz])             { return RevPutStr(rbuf, StrView{chary}); }
template <class RevBuffer, size_t arysz> inline void RevPutStr(RevBuffer& rbuf, const char (&chary)[arysz], FmtDef fmt) { return RevPutStr(rbuf, StrView{chary}, fmt); }
template <class RevBuffer, size_t arysz> inline void RevPutStr(RevBuffer& rbuf, char (&cstr)[arysz])                    { return RevPutStr(rbuf, StrView_eos_or_all(cstr)); }
template <class RevBuffer, size_t arysz> inline void RevPutStr(RevBuffer& rbuf, char (&cstr)[arysz], FmtDef fmt)        { return RevPutStr(rbuf, StrView_eos_or_all(cstr), fmt); }

template <class RevBuffer> inline void RevPutStr(RevBuffer& rbuf, char ch)             { return RevPutChar(rbuf, ch); }
template <class RevBuffer> inline void RevPutStr(RevBuffer& rbuf, char ch, FmtDef fmt) { return RevPutStr(rbuf, StrView{&ch,1}, fmt); }

template <class RevBuffer, class ValueT>
inline auto RevPutStr(RevBuffer& rbuf, ValueT&& value) -> decltype(RevPutStr(rbuf, ToStrView(value)), void()) {
   return RevPutStr(rbuf, ToStrView(value));
}

template <class RevBuffer, class ValueT, class FmtT>
inline auto RevPutStr(RevBuffer& rbuf, ValueT&& value, FmtT fmt) -> decltype(RevPutStr(rbuf, ToStrView(value), std::forward<FmtT>(fmt))) {
   return RevPutStr(rbuf, ToStrView(value), fmt);
}

//--------------------------------------------------------------------------//

template <class RevBuffer, class ValueT>
inline auto RevPutStr(RevBuffer& rbuf, ValueT value) -> decltype(ToStrRev(static_cast<char*>(nullptr), value), void()) {
   char* pout = rbuf.AllocPrefix(sizeof(NumOutBuf));
   pout = ToStrRev(pout, value);
   rbuf.SetPrefixUsed(pout);
}

template <class RevBufferT, class ValueT, class FmtT>
inline auto RevPutStr(RevBufferT& rbuf, ValueT value, const FmtT& fmt) -> decltype(ToStrRev(static_cast<char*>(nullptr), value, fmt), void()) {
   char* pout = rbuf.AllocPrefix(std::max(static_cast<unsigned>(sizeof(NumOutBuf) + fmt.Precision_), static_cast<unsigned>(fmt.Width_)));
   pout = ToStrRev(pout, value, fmt);
   rbuf.SetPrefixUsed(pout);
}

//--------------------------------------------------------------------------//

template <class RevBuffer>
inline void RevPrint(RevBuffer&) {
}

/// \ingroup Buffer
/// 利用 RevPut(), ToStrRev() 把 value 轉成字串填入 rbuf.
template <class RevBuffer, class T, class... ArgsT>
inline void RevPrint(RevBuffer& rbuf, T&& value, ArgsT&&... args) {
   RevPrint(rbuf, std::forward<ArgsT>(args)...);
   RevPutStr(rbuf, std::forward<T>(value));
}

template <class RevBuffer, class T, class FmtT, class... ArgsT>
inline auto RevPrint(RevBuffer& rbuf, T&& value, FmtT&& fmt, ArgsT&&... args)
-> decltype(RevPutStr(rbuf, std::forward<T>(value), std::forward<FmtT>(fmt))) {
   RevPrint(rbuf, std::forward<ArgsT>(args)...);
   RevPutStr(rbuf, std::forward<T>(value), std::forward<FmtT>(fmt));
}

} // namespace
#endif//__fon9_buffer_RevPrint_hpp__
