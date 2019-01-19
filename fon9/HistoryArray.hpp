// \file fon9/HistoryArray.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_HistoryArray_hpp__
#define __fon9_HistoryArray_hpp__
#include "fon9/sys/Config.h"

namespace fon9 {

fon9_WARN_DISABLE_PADDING;
// (4625:copy ctor/4626:assignment operator/5026:move ctor/5027:move assignment operator) was implicitly defined as deleted
fon9_MSC_WARN_DISABLE_NO_PUSH(4625 4626 5026 5027);

/// \ingroup Misc
/// 最多保存 historyCount 筆資料.
/// - 要新增資料時透過 T& AppendRec(); 增加最後一筆. 此時的資料內容可能為之前的資料.
/// - 透過 GetLastRec() 可取得最後增加的資料,
///   若尚未執行過 AppendRec() 則 GetLastRec() 取出的是 T 的初始值(可能未定義).
template <class T, unsigned historyCount>
class HistoryArray {
   T        Recs_[historyCount];
   unsigned LastIdx_{0};
public:
   using Type = T;
   enum : unsigned {
      kHistoryCount = historyCount
   };

   T& AppendRec() {
      this->LastIdx_ = (this->LastIdx_ == 0 ? (kHistoryCount - 1) : (this->LastIdx_ - 1));
      return this->Recs_[this->LastIdx_];
   }

   T& GetLastRec() {
      return this->Recs_[this->LastIdx_];
   }
   const T& GetLastRec() const {
      return this->Recs_[this->LastIdx_];
   }
   /// p=0 最後一個, p=1 倒數第2個...
   const T& GetLastRec(unsigned p) const {
      assert(p < kHistoryCount);
      return this->Recs_[(this->LastIdx_ + p) % kHistoryCount];
   }
};
fon9_WARN_POP;

} // namespaces
#endif//__fon9_HistoryArray_hpp__
