/// \file fon9/DyObj.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_DyObj_hpp__
#define __fon9_DyObj_hpp__
#include "fon9/Utility.hpp"

namespace fon9 {

fon9_WARN_DISABLE_PADDING;
/// \ingroup Misc
/// 預先保留 T 的空間, 在必要時才建立 T.
/// 在 ~DyObj() 解構時, 如果 T 物件存在, 則自動解構T.
template <class T>
class DyObj {
   byte  ValueBuf_[sizeof(T)] alignas(T);
   bool  HasValue_{false};
public:
   using value_type = T;

   DyObj() : HasValue_{false} {
   }

   ~DyObj() {
      this->clear();
   }

   DyObj(const DyObj& rhs) : HasValue_{rhs.HasValue_} {
      if (const value_type* prval = rhs.get())
         InplaceNew<value_type>(this->ValueBuf_, *prval);
   }
   DyObj& operator= (const DyObj& rhs) {
      const value_type* prval = rhs.get();
      if (this->HasValue_) {
         if (prval)
            *reinterpret_cast<value_type*>(this->ValueBuf_) = *prval;
         else
            this->clear();
      }
      else if (prval) {
         this->HasValue_ = true;
         InplaceNew<value_type>(this->ValueBuf_, *prval);
      }
      return *this;
   }

   DyObj(DyObj&& rhs) : HasValue_{rhs.HasValue_} {
      if (this->HasValue_) {
         InplaceNew<value_type>(this->ValueBuf_, std::move(*reinterpret_cast<value_type*>(rhs.ValueBuf_)));
         rhs.clear();
      }
   }
   DyObj& operator= (DyObj&& rhs) {
      if (this->HasValue_) {
         if (rhs.HasValue_) {
            *reinterpret_cast<value_type*>(this->ValueBuf_) = std::move(*reinterpret_cast<value_type*>(rhs.ValueBuf_));
            rhs.clear();
         }
         else
            this->clear();
      }
      else if (rhs.HasValue_) {
         this->HasValue_ = true;
         InplaceNew<value_type>(this->ValueBuf_, std::move(*reinterpret_cast<value_type*>(rhs.ValueBuf_)));
         rhs.clear();
      }
      return *this;
   }

   template <class... ArgsT>
   value_type* emplace(ArgsT&&... args) {
      if (value_type* val = this->get())
         return val;
      InplaceNew<value_type>(this->ValueBuf_, std::forward<ArgsT>(args)...);
      this->HasValue_ = true;
      return reinterpret_cast<value_type*>(this->ValueBuf_);
   }
   template <class... ArgsT>
   value_type* renew(ArgsT&&... args) {
      this->clear();
      InplaceNew<value_type>(this->ValueBuf_, std::forward<ArgsT>(args)...);
      this->HasValue_ = true;
      return reinterpret_cast<value_type*>(this->ValueBuf_);
   }
   size_t clear() {
      if (value_type* val = this->get()) {
         this->HasValue_ = false;
         val->~value_type();
         return 1;
      }
      return 0;
   }
   value_type* get() {
      return this->HasValue_ ? reinterpret_cast<value_type*>(this->ValueBuf_) : nullptr;
   }
   const value_type* get() const {
      return this->HasValue_ ? reinterpret_cast<const value_type*>(this->ValueBuf_) : nullptr;
   }
   value_type* operator->() {
      return this->get();
   }
   const value_type* operator->() const {
      return this->get();
   }
   value_type& operator*() {
      return *this->get();
   }
   const value_type& operator*() const {
      return *this->get();
   }

   static DyObj& StaticCast(value_type& val) {
      return ContainerOf(*reinterpret_cast<byte(*)[sizeof(T)]>(&val), &DyObj::ValueBuf_);
   }
   static const DyObj& StaticCast(const value_type& val) {
      return StaticCast(*const_cast<value_type*>(&val));
   }
};
fon9_WARN_POP;

} // namespace
#endif//__fon9_DyObj_hpp__
