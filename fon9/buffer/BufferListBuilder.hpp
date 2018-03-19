/// \file fon9/buffer/BufferListBuilder.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_buffer_BufferListBuilder_hpp__
#define __fon9_buffer_BufferListBuilder_hpp__
#include "fon9/buffer/BufferList.hpp"

namespace fon9 {

fon9_WARN_DISABLE_PADDING;
/// \ingroup Buffer
/// RevBufferList; FwdBufferList; 的基底類別, 一般而言:
/// - 使用字串輸出時, 使用 RevBufferList: 由後往前加入資料, 例: 文字 log 記錄.
/// - 使用固定 size 的 binary 輸出時, 使用 FwdBufferList: 由前往後加入資料.
class BufferListBuilder {
public:
   /// \param newAllocReserved
   ///            當現有節點空間不足,需要分配新節點時,新節點最少需要額外保留多少空間.
   ///            - 較大的值: 可以降低分配新節點機率, 但會比較浪費記憶體.
   ///            - 較小的值: 可能增加分配新節點機率, 但會比較節省記憶體.
   explicit BufferListBuilder(BufferNodeSize newAllocReserved) : NewAllocReserved_{newAllocReserved} {
   }
   /// 將 buf 移入後繼續增加資料.
   /// - 如果衍生類=RevBufferList: 則新資料會放在 buf 的前面.
   /// - 如果衍生類=FwdBufferList: 則新資料會放在 buf 的後面.
   BufferListBuilder(BufferNodeSize newAllocReserved, BufferList&& buf)
      : NewAllocReserved_{newAllocReserved}
      , List_{std::move(buf)} {
   }
   BufferListBuilder(BufferListBuilder&&) = default;
   BufferListBuilder& operator=(BufferListBuilder&&) = default;
   BufferListBuilder(BufferListBuilder&) = delete;
   BufferListBuilder& operator=(BufferListBuilder&) = delete;

   const BufferNode* cfront() const {
      return this->List_.front();
   }
   BufferNodeSize GetNewAllocReserved() const {
      return this->NewAllocReserved_;
   }

protected:
   ~BufferListBuilder() {
   }
   BufferList MoveOut() {
      return std::move(this->List_);
   }

   BufferNodeSize NewAllocReserved_;
   BufferList     List_;
};
fon9_WARN_POP;

} // namespaces
#endif//__fon9_buffer_BufferListBuilder_hpp__
