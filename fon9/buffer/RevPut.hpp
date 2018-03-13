/// \file fon9/buffer/RevPut.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_buffer_RevPut_hpp__
#define __fon9_buffer_RevPut_hpp__
#include "fon9/buffer/RevBuffer.hpp"
#include <string.h>

namespace fon9 {

template <class RevBuffer> inline void RevPutChar(RevBuffer& rbuf, char ch) {
   char* pout = rbuf.AllocPrefix(1);
   *--pout = ch;
   rbuf.SetPrefixUsed(pout);
}

template <class RevBuffer> inline void RevPutFill(RevBuffer& rbuf, size_t sz, char ch) {
   if (fon9_LIKELY(sz > 0)) {
      char* pout = rbuf.AllocPrefix(sz);
      rbuf.SetPrefixUsed(reinterpret_cast<char*>(memset(pout - sz, ch, sz)));
   }
}

template <class RevBuffer> inline void RevPutMem(RevBuffer& rbuf, const void* mem, size_t sz) {
   if (fon9_LIKELY(sz > 0)) {
      char* pout = rbuf.AllocPrefix(sz);
      rbuf.SetPrefixUsed(reinterpret_cast<char*>(memcpy(pout - sz, mem, sz)));
   }
}

template <class RevBuffer> inline void RevPutMem(RevBuffer& rbuf, const void* beg, const void* end) {
   return RevPutMem(rbuf, beg, static_cast<size_t>(reinterpret_cast<const char*>(end) - reinterpret_cast<const char*>(beg)));
}

/// 若 chKVSpkutter = '='; chRecordSplitter = '|';
/// 填入: "k=v" or "k1=v1|k2=v2"...
/// 尾端及頭端沒有 chRecordSplitter 字元.
template <class RevBuffer, class Map>
void RevPutMapToStr(RevBuffer& rbuf, const Map& src, char chKVSplitter, char chRecordSplitter) {
   if (src.empty())
      return;
   const auto ibeg = src.begin();
   auto iend = src.end();
   for (;;) {
      const auto& iv = *(--iend);
      RevPrint(rbuf, iv.first, chKVSplitter, iv.second);
      if (ibeg == iend)
         break;
      RevPutChar(rbuf, chRecordSplitter);
   }
}

} // namespace
#endif//__fon9_buffer_RevPut_hpp__
