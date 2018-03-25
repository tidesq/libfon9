/// \file fon9/MustLock.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_MustLock_hpp__
#define __fon9_MustLock_hpp__
#include "fon9/Utility.hpp"
fon9_BEFORE_INCLUDE_STD;
#include <mutex>
fon9_AFTER_INCLUDE_STD;

namespace fon9 {

fon9_WARN_DISABLE_PADDING;
/// \ingroup Thrs
/// 必須先鎖定, 才能使用 BaseT.
template <class BaseT,
   class MutexT = std::mutex,
   class MutexLockerT = std::unique_lock<MutexT>>
class MustLock {
   fon9_NON_COPY_NON_MOVE(MustLock);
   MutexT mutable Mutex_;
   BaseT          Base_;
public:
   using MutexLocker = MutexLockerT;

   template <class... ArgsT>
   MustLock(ArgsT&&... args) : Base_{std::forward<ArgsT>(args)...} {
   }

   /// 必須透過鎖定才能取得 LockedBaseT 物件.
   template <class OwnerT, class LockedBaseT>
   class LockerT : public MutexLockerT {
      fon9_NON_COPYABLE(LockerT);
      using base = MutexLockerT;
      OwnerT*  Owner_;
   public:
      LockerT() : Owner_{nullptr} {}
      LockerT(OwnerT& owner) : base{owner.Mutex_}, Owner_{&owner} {}
      LockerT(LockerT&& other) : base{std::move(other)}, Owner_{other.Owner_} { other.Owner_ = nullptr; }
      LockerT& operator=(LockerT&& other) {
         base::operator=(std::move(other));
         this->Owner_ = other.Owner_;
         other.Owner_ = nullptr;
         return *this;
      }
      /// 必須在鎖定狀態下才能呼叫, 否則資料可能會被破壞, 甚至造成 crash!
      LockedBaseT* operator->() const {
         assert(this->owns_lock());
         return &this->Owner_->Base_;
      }
      /// 必須在鎖定狀態下才能呼叫, 否則資料可能會被破壞, 甚至造成 crash!
      LockedBaseT& operator*() const {
         assert(this->owns_lock());
         return this->Owner_->Base_;
      }
      /// 取出的物件必須小心使用, 如果您呼叫了 unlock(); 則在 lock() 之前, 都不可使用他.
      LockedBaseT* get() const { return &this->Owner_->Base_; }
      OwnerT* GetOwner() const { return this->Owner_; }
   };
   using Locker = LockerT<MustLock, BaseT>;
   using ConstLocker = LockerT<const MustLock, const BaseT>;

   Locker Lock() { return Locker{*this}; }
   ConstLocker Lock() const { return ConstLocker{*this}; }
   ConstLocker ConstLock() const { return ConstLocker{*this}; }

   static MustLock& StaticCast(BaseT& pbase) {
      return ContainerOf(pbase, &MustLock::Base_);
   }
   static const MustLock& StaticCast(const BaseT& pbase) {
      return ContainerOf(*const_cast<BaseT*>(&pbase), &MustLock::Base_);
   }
};
fon9_WARN_POP;

} // namespaces
#endif//__fon9_MustLock_hpp__
