/// \file fon9/seed/NamedPark.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_NamedPark_hpp__
#define __fon9_seed_NamedPark_hpp__
#include "fon9/seed/MaTree.hpp"
#include "fon9/Subr.hpp"

namespace fon9 { namespace seed {

/// \ingroup seed
/// - 使用 NamedSeed 當作 Park 裡面的元素.
///   - e.g. DevicePark(NamedPark.Sapling = ParkTree) 裡面有: TcpClientFactory, TcpServerFactory
/// - 在「加入」、「移除」時, 提供事件通知.
/// - 在事件通知時, 禁止在相同 thread 再次呼叫: Subscribe(), Unsubscibe(), Add(), Remove();
/// - 在 OnParentSeedClear() 會觸發 EventType::Clear 事件.
class fon9_API ParkTree : public MaTree {
   fon9_NON_COPY_NON_MOVE(ParkTree);
   using base = MaTree;
public:
   enum class EventType {
      Add,
      Remove,
      Clear,
   };
   using FnEventHandler = std::function<void(NamedSeed*, EventType)>;

   using base::base;

   /// 註冊異動事件通知.
   template <class... ArgsT>
   SubConn Subscribe(ArgsT&&... args) {
      Locker locker{this->Container_};
      return this->Subj_.Subscribe(std::forward<ArgsT>(args)...);
   }
   /// 移除異動事件通知.
   size_t Unsubscribe(SubConn subConn) {
      Locker locker{this->Container_};
      return this->Subj_.Unsubscribe(subConn);
   }
private:
   void OnMaTree_AfterRemove(Locker&, NamedSeed& seed) override;
   void OnMaTree_AfterAdd(Locker&, NamedSeed& seed) override;
   void OnMaTree_AfterClear() override;
   
   // 使用 Locker locker{this->Container_}; 所以這裡用 UnsafeSubject<>
   using Subj = UnsafeSubject<FnEventHandler>;
   Subj  Subj_;
};

/// \ingroup seed
/// \copydoc ParkTree
template <class ObjectT>
class NamedPark : public NamedSapling {
   using base = NamedSapling;
public:
   using ObjectSP = NamedSeedSPT<ObjectT>;
   using FnEventHandler = std::function<void(ObjectT*, ParkTree::EventType)>;

   NamedPark(StrView name) : base{name.ToString(), new ParkTree{name.ToString()}} {
   }

   /// 把 obj 加入 Sapling(MaTree), 在返回前會先觸發 EventType_Add.
   bool Add(ObjectSP obj) {
      return static_cast<ParkTree*>(this->Sapling_.get())->Add(std::move(obj));
   }
   /// 把 obj 從 Sapling(MaTree) 移除, 在返回前會先觸發 EventType::Remove.
   ObjectSP Remove(const StrView& name) {
      return static_pointer_cast<ObjectT>(static_cast<ParkTree*>(this->Sapling_.get())->Remove(name));
   }
   ObjectSP Get(const StrView& name) {
      return static_cast<ParkTree*>(this->Sapling_.get())->Get<ObjectT>(name);
   }

   /// 註冊異動事件通知.
   template <class... ArgsT>
   SubConn Subscribe(ArgsT&&... args) {
      FnEventHandler subr{std::forward<ArgsT>(args)...};
      return static_cast<ParkTree*>(this->Sapling_.get())->Subscribe(std::move(*reinterpret_cast<ParkTree::FnEventHandler*>(&subr)));
   }
   /// 移除異動事件通知.
   size_t Unsubscribe(SubConn subConn) {
      return static_cast<ParkTree*>(this->Sapling_.get())->Unsubscribe(subConn);
   }
};

template <class ParkT, class... ArgsT>
inline NamedSeedSPT<ParkT> FetchNamedPark(MaTree& maTree, StrView parkName, ArgsT&&... ctorArgs) {
   NamedSeedSPT<ParkT> newPark;
   for (;;) {
      NamedSeedSPT<ParkT> park = maTree.Get<ParkT>(parkName);
      if (park)
         return park;
      if (fon9_LIKELY(!newPark))
         newPark.reset(new ParkT{maTree, parkName, std::forward<ArgsT>(ctorArgs)...});
      if (fon9_LIKELY(maTree.Add(newPark)))
         return newPark;
      // maTree.Add(newPark) 失敗, 另一 thread 同時 Add()!
   }
}

} } // namespaces
#endif//__fon9_seed_NamedPark_hpp__
