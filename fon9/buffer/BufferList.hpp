/// \file fon9/buffer/BufferList.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_buffer_BufferList_hpp__
#define __fon9_buffer_BufferList_hpp__
#include "fon9/buffer/BufferNode.hpp"

namespace fon9 {

/// \ingroup Buffer
/// 使用 BufferNode 組成的單向串列.
/// - 大幅度降低資料複製.
using BufferList = SinglyLinkedList2<BufferNode>;

/// \ingroup Buffer
/// 把 buf 的全部內容, 透過 str.reserve(); str.append() 加入 str.
/// buf 內容保持不變.
template <class StrT>
inline void BufferAppendTo(const BufferList& buf, StrT& str) {
   if (const BufferNode* front = buf.front()) {
      str.reserve(str.size() + CalcDataSize(front));
      do {
         str.append(reinterpret_cast<const char*>(front->GetDataBegin()), front->GetDataSize());
      } while ((front = front->GetNext()) != nullptr);
   }
}

/// \ingroup Buffer
/// 把 buf 的全部內容放到 str 尾端, buf 內容保持不變.
fon9_API void BufferAppendTo(const BufferList& buf, std::string& str);

/// \ingroup Buffer
/// BufferTo<std::string>(buf); 可以把 buf 的全部內容變成字串後返回, buf 內容保持不變.
template <class StrT>
inline StrT BufferTo(const BufferList& buf) {
   StrT str;
   BufferAppendTo(buf, str);
   return str;
}

/// \ingroup Buffer
/// 如果 dst.back() 足夠存放 src 則直接複製到 dst.back(), 否則在 dst 後面增加一個節點存放 src.
fon9_API void AppendToBuffer(BufferList& dst, const void* src, size_t size);

} // namespaces
#endif//__fon9_buffer_BufferList_hpp__
