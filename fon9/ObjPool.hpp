/// \file fon9/ObjPool.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_ObjPool_hpp__
#define __fon9_ObjPool_hpp__
#include "fon9/sys/Config.hpp"

namespace fon9 {

template <class T>
class ObjPool {
public:
   using ContainerT = std::vector<T>;
   using SizeT = typename ContainerT::size_type;

   ObjPool(SizeT reserveSize) {
      this->Reserve(reserveSize);
   }
   ContainerT MoveOut() {
      this->FreeIndex_.clear();
      return std::move(this->Objs_);
   }

   /// 如果在 GetPtrObj() 之後有 Add() 或 Remove() 則不保證 retval 持續有效!
   T* GetObjPtr(SizeT idx) {
      if (idx >= this->Objs_.size())
         return nullptr;
      return &this->Objs_[idx];
   }
   /// 取得一份 T 的複製.
   T GetObj(SizeT idx) {
      if (idx >= this->Objs_.size())
         return T{};
      return this->Objs_[idx];
   }

   /// \return 新加入的 obj 所在的 index.
   template <class X>
   SizeT Add(X obj) {
      SizeT idx;
      if (this->FreeIndex_.empty()) {
         idx = this->Objs_.size();
         this->Objs_.emplace_back(std::move(obj));
      }
      else {
         idx = this->FreeIndex_.back();
         this->FreeIndex_.pop_back();
         this->Objs_[idx] = std::move(obj);
      }
      return idx;
   }

   template <class X>
   bool RemoveObj(SizeT idx, X obj) {
      if (idx >= this->Objs_.size())
         return false;
      T& vele = this->Objs_[idx];
      if (vele != obj)
         return false;
      vele = T{};
      this->FreeIndex_.push_back(idx);
      return true;
   }

   /// pobj 必須是透過 GetPtrObj(idx) 取得, 且 pobj 必須已經清理過(釋放資源).
   bool RemoveObjPtr(SizeT idx, T* pobj) {
      if (idx >= this->Objs_.size())
         return false;
      if (pobj != &this->Objs_[idx])
         return false;
      this->FreeIndex_.push_back(idx);
      return true;
   }

   void Reserve(SizeT sz) {
      this->Objs_.reserve(sz);
      this->FreeIndex_.reserve(sz);
   }

   SizeT Size() const {
      return this->Objs_.size() - this->FreeIndex_.size();
   }

private:
   ContainerT           Objs_;
   std::vector<SizeT>   FreeIndex_;
};

} // namespaces
#endif//__fon9_ObjPool_hpp__
