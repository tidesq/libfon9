// \file fon9/StaticPtr.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_StaticPtr_hpp__
#define __fon9_StaticPtr_hpp__
#include "fon9/sys/Config.hpp"

namespace fon9 {

/// \ingroup Misc
/// 取代 `static std::unique_ptr<T> ptr;` or `static thread_local std::unique_ptr<T> ptr;`
/// 因為在 ptr 死亡後, 可能還會用到 ptr ?!
/// 增加 IsDisposed() 判斷 ptr 本身(不是所指物件), 是否已經死亡!
template <class T>
class StaticPtr {
   fon9_NON_COPY_NON_MOVE(StaticPtr);
   T* Ptr_;
public:
   StaticPtr(T* p = nullptr) : Ptr_{p} {
   }
   ~StaticPtr() {
      this->dispose();
   }
   void dispose() {
      this->reset(reinterpret_cast<T*>(1));
   }
   void reset(T* p) {
      std::swap(p, this->Ptr_);
      if (reinterpret_cast<uintptr_t>(p) > 1)
         delete p;
      // 不可直接使用 this->Ptr_ = p;
      // 因為解構 ~StaticPtr() 時呼叫 reset() 時 compiler 可能因最佳化:
      // 造成省略該步驟 this->Ptr_ 仍維持舊資料,
      // 而在 MemBlock::Alloc(size_t sz) 裡面無法正確判斷 IsDisposed();
   }
   T* operator->() {
      return this->Ptr_;
   }
   bool operator!() const {
      return reinterpret_cast<uintptr_t>(this->Ptr_) <= 1;
   }
   explicit operator bool() const {
      return !this->operator!();
   }
   bool IsDisposed() const {
      return reinterpret_cast<uintptr_t>(this->Ptr_) == 1;
   }
};

} // namespace fon9
#endif//__fon9_StaticPtr_hpp__
