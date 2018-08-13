/// \file fon9/Subr.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_Subr_hpp__
#define __fon9_Subr_hpp__
#include "fon9/DummyMutex.hpp"
#include "fon9/MustLock.hpp"
#include "fon9/SortedVector.hpp"

namespace fon9 {

/// Subject/Subscriber 註冊取得的 ID, 用來取消註冊時使用.
/// - 此ID的編碼方式為: 在新的訂閱者註冊時, 為目前最後一個ID+1, 如果現在沒有任何訂閱者則使用1.
/// - 所以如果最後一個訂閱者為 0xffffffff... 則無法再註冊新的訂閱者, 此時註冊就會失敗傳回 nullptr
using SubConn = struct SubId*;

fon9_WARN_DISABLE_PADDING;
/// 提供給 Subject 儲存訂閱者的容器.
template <class SubscriberT>
class SubrMap : private SortedVector<SubConn, SubscriberT> {
   using base = SortedVector<SubConn, SubscriberT>;
   unsigned FirstReserve_;
public:
   using typename base::size_type;
   using typename base::iterator;
   using typename base::difference_type;
   using typename base::value_type;

   /// \param firstReserve 當加入第一個元素時的預分配資料量,0表示使用預設值.
   /// - 第一個 subr 註冊時, 表示此 Subject 是讓人感興趣的.
   /// - 可以預期, 接下來可能還會有別人訂閱.
   /// - 預先分配 **一小塊** 可能的空間, 可大幅改進後續訂閱者的處理速度.
   /// - 在 x64 系統裡: 如果 SubscriberT=Object*; 加上 SubId; 每個 T 占用 sizeof(void*)*2 = 16;
   /// - 所以如果 firstReserve=64, 則加入第一個 subr 時大約占用 16 bytes * 64 items = 1024 bytes.
   SubrMap(unsigned firstReserve) : FirstReserve_{firstReserve} {
   }

   using base::begin;
   using base::end;
   using base::erase;
   using base::size;
   using base::sindex;
   /// 使用 SortedVector::find(SubConn id) 進行二元搜尋.
   iterator find(SubConn id) {
      return base::find(id);
   }
   /// 使用 std::find(subr) 線性搜尋, subr 必須提供 `==` 操作.
   template <class SubscriberT2>
   iterator find(const SubscriberT2& subr) {
      return std::find_if(this->begin(), this->end(), [&subr](const value_type& i) {
         return(i.second == subr);
      });
   }
   /// 移除全部相同的 subr, 傳回移除的數量.
   template <class SubscriberT2>
   size_type remove(const SubscriberT2& subr, size_type ibegin, size_type iend) {
      return this->remove_if(ibegin, iend, [&subr](const value_type& i) -> bool {
                     return(i.second == subr);
                  });
   }
   /// 在尾端建立一個 Subscriber.
   /// \return 傳回nullptr表示失敗, 請參閱 \ref SubConn
   template <class... ArgsT>
   SubConn emplace_back(ArgsT&&... args) {
      SubConn id;
      if (fon9_LIKELY(!this->empty())) {
         SubConn lastid = this->back().first;
         id = reinterpret_cast<SubConn>(reinterpret_cast<uintptr_t>(lastid) + 1);
         if (fon9_UNLIKELY(id < lastid))
            return nullptr;
      }
      else {
         id = reinterpret_cast<SubConn>(1);
         if (this->FirstReserve_)
            this->reserve(this->FirstReserve_);
      }
      base::emplace_back(id, std::forward<ArgsT>(args)...);
      return id;
   }
};

/// 事件訂閱機制: 訂閱事件的主題.
/// \tparam SubscriberT 訂閱者型別, 必須配合 Publish(args...) 的參數, 提供 SubscriberT(args...) 呼叫.
///                     針對 thread safe 的議題, 可考慮 std::function + shared_ptr + weak_ptr.
/// \tparam MutexT      預設使用 std::recursive_mutex: 允許在收到訊息的時候, 在同一個 thread 直接取消訂閱!
/// \tparam ContainerT  儲存訂閱者的容器: 預設使用 SubrMap; 請參考 SubrMap 的介面.
template < class SubscriberT
   , class MutexT = std::recursive_mutex
   , template <class SubscriberT> class ContainerT = SubrMap
   >
class Subject {
   fon9_NON_COPY_NON_MOVE(Subject);
   using ContainerImpl = ContainerT<SubscriberT>;
   using Container = MustLock<ContainerImpl, MutexT>;
   using Locker = typename Container::Locker;
   using iterator = typename ContainerImpl::iterator;
   using size_type = typename ContainerImpl::size_type;
   using difference_type = typename ContainerImpl::difference_type;
   Container   Subrs_;
   size_type   NextEmitIdx_{};
   size_type EraseIterator(Locker& subrs, iterator ifind) {
      if (ifind == subrs->end())
         return 0;
      size_type idx = static_cast<size_type>(ifind - subrs->begin());
      if (idx < this->NextEmitIdx_)
         --this->NextEmitIdx_;
      subrs->erase(ifind);
      return 1;
   }
public:
   using Subscriber = SubscriberT;

   /// \copydoc SubrMap::SubrMap
   explicit Subject(unsigned firstReserve = 32) : Subrs_{firstReserve} {
   }

   /// 直接在尾端註冊一個新的訂閱者.
   /// - 註冊時不會檢查是否已經有相同訂閱者, 也就是: 同一個訂閱者, 可以註冊多次, 此時同一次發行流程, 會收到多次訂閱的事件.
   /// - 使用 SubscriberT{args...} 直接在尾端建立訂閱者.
   /// - 如果 SubscriberT 不支援 `operator==(const SubscriberT& subr)`, 則傳回值是唯一可以取消訂閱的依據.
   ///
   /// \return 傳回值是取消訂閱時的依據, 傳回 nullptr 表示無法註冊, 原因請參閱 \ref SubConn 的說明.
   template <class... ArgsT>
   SubConn Subscribe(ArgsT&&... args) {
      Locker subrs(this->Subrs_);
      return subrs->emplace_back(std::forward<ArgsT>(args)...);
   }
   /// 取消訂閱.
   /// - 若 MutexT = std::recursive_mutex 則: 可以在收到訊息的時候, 在同一個 thread 之中取消訂閱!
   /// - 若 MutexT = std::mutex 則: 在收到訊息的時候, 在同一個 thread 之中取消訂閱: 會死結!
   ///
   /// \param connection 是 Subscribe() 的傳回值.
   /// \return 傳回實際移除的數量: 0 or 1
   size_type Unsubscribe(SubConn connection) {
      if (!connection)
         return 0;
      Locker subrs(this->Subrs_);
      return this->EraseIterator(subrs, subrs->find(connection));
   }
   /// 取消訂閱.
   /// \param subr  SubscriberT 必須支援: bool operator==(const SubscriberT&) const;
   /// \return 傳回實際移除的數量: 0 or 1
   size_type Unsubscribe(const SubscriberT& subr) {
      Locker subrs(this->Subrs_);
      return this->EraseIterator(subrs, subrs->find(subr));
   }
   size_type UnsubscribeAll(const SubscriberT& subr) {
      Locker      subrs(this->Subrs_);
      size_type   count = subrs->remove(subr, this->NextEmitIdx_, subrs->size());
      if (this->NextEmitIdx_ > 0) {
         if (size_type count2 = subrs->remove(subr, 0, this->NextEmitIdx_)) {
            this->NextEmitIdx_ -= count2;
            count += count2;
         }
      }
      return count;
   }
   /// - 在發行訊息的過程, 會 **全程鎖住容器**.
   /// - 這裡會呼叫 subr(std::forward<ArgsT>(args)...);
   /// - 在收到發行訊息時:
   ///   - 若 MutexT = std::mutex; 則禁止在同一個 thread: 重複發行 or 取消註冊 or 新增註冊. 如果您這樣做, 會立即進入鎖死狀態!
   ///   - 若 MutexT = std::recursive_mutex:
   ///      - 可允許: 重複發行 or 取消註冊 or 新增註冊.
   ///      - 請避免: 在重複發行時, 又取消註冊. 因為原本的發行流程, 可能會有訂閱者遺漏訊息, 例:
   ///         - Pub(msgA): Subr1(idx=0), Subr2(idx=1), Subr3(idx=2), Subr4(idx=3)
   ///         - 在 Subr2 收到 msgA 時(msgA.NextIdx=2), Pub(msgB), 當 Subr1 收到 msgB 時, 取消註冊 Subr1.
   ///         - 此時剩下 Subr2(idx=0), Subr3(idx=1), Subr4(idx=2)
   ///         - 接下來 msgB 的流程: Subr2, Subr3, Subr4; msgB 發行結束.
   ///         - 返回 msgA(msgA.NextIdx=2) 繼續流程: Subr4; **此時遺漏了 Subr3**
   template <class... ArgsT>
   void Publish(ArgsT&&... args) {
      struct Combiner {
         bool operator()(SubscriberT& subr, ArgsT&&... args) {
            subr(std::forward<ArgsT>(args)...);
            return true;
         }
      } combiner;
      Combine(combiner, std::forward<ArgsT>(args)...);
   }
   /// 透過 combiner 合併訊息發行結果.
   /// 若 combiner(SubscriberT& subr, ArgsT&&... args) 傳回 false 則中斷發行.
   template <class CombinerT, class... ArgsT>
   void Combine(CombinerT& combiner, ArgsT&&... args) {
      Locker subrs(this->Subrs_);
      struct ResetIdx {
         size_type  prv;
         size_type* idx;
         ResetIdx(size_type* i) : prv{*i}, idx{i} {
            *i = 0;
         }
         ~ResetIdx() { *idx = prv; }
      } resetIdx{&this->NextEmitIdx_};
      while (this->NextEmitIdx_ < subrs->size()) {
         if (!combiner(subrs->sindex(this->NextEmitIdx_++).second, std::forward<ArgsT>(args)...))
            break;
      }
   }
   /// 是否沒有訂閱者?
   /// 若 MutexT = std::mutex 則: 在收到訊息的時候, 在同一個 thread 之中呼叫: 會死結!
   bool IsEmpty() const {
      Locker subrs(this->Subrs_);
      return subrs->empty();
   }
   /// 訂閱者數量.
   size_type GetSubscriberCount() const {
      auto   subrs(this->Subrs_.Lock());
      return subrs->size();
   }
};

/// 不支援 thread safe 的 Subject: 使用 fon9::DummyMutex.
template <class SubscriberT, template <class T> class SubrContainerT = SubrMap>
using UnsafeSubject = Subject<SubscriberT, DummyMutex, SubrContainerT>;

/// - 輔助 Subject 提供 Obj* 當作 Subscriber 使用.
///   \code
///      struct CbObj {
///         virtual void operator()() = 0;
///      };
///      using Subr = fon9::ObjCallback<CbObj>;
///      using Subj = fon9::Subject<Subr>;
///   \endcode
/// - 提供 Obj* 呼叫 Obj::operator() 的包裝.
/// - 提供 operator==(); operator!=() 判斷是否指向同一個 Obj 
template <class Obj>
struct ObjCallback {
   Obj* Obj_;
   ObjCallback(Obj* obj) : Obj_{obj} {
   }
   bool operator==(const ObjCallback& r) const {
      return this->Obj_ == r.Obj_;
   }
   bool operator!=(const ObjCallback& r) const {
      return this->Obj_ == r.Obj_;
   }
   template <class... ArgsT>
   auto operator()(ArgsT&&... args) const -> decltype((*this->Obj_)(std::forward<ArgsT>(args)...)) {
      return (*this->Obj_)(std::forward<ArgsT>(args)...);
   }
};
fon9_WARN_POP;

} // namespaces
#endif//__fon9_Subr_hpp__
