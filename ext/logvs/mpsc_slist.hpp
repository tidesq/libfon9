/// \file fon9/mpsc_slist.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_mpsc_slist_hpp__
#define __fon9_mpsc_slist_hpp__
#include "fon9/sys/Config.hpp"
#include <atomic>

namespace fon9 {

/// 原始來源 http://www.1024cores.net/home/lock-free-algorithms/queues/intrusive-mpsc-node-based-queue
/// https://github.com/RafaGago/mini-async-log/blob/master/src/mal_log/util/mpsc.hpp
class SListMPSC {
   fon9_NON_COPY_NON_MOVE(SListMPSC);
public:
   class Node;
   using ANodePtr = std::atomic<Node*>;
   class Node {
      fon9_NON_COPY_NON_MOVE(Node);
      friend class SListMPSC;
      union {
         Node*    UnsafeNext_;
         ANodePtr AtomicNext_;
      };
      Node(Node* next) : UnsafeNext_{next} {
         static_assert(sizeof(this->UnsafeNext_) == sizeof(this->AtomicNext_), "atomic<Node*> is not lock-free?");
      }
   public:
      Node() : UnsafeNext_{nullptr} {
      }
   };

   SListMPSC() : Head_{&Stub_}, Back_{&Stub_} {
   }
   ~SListMPSC() {
      // FreeList(this->Head_);
   }

   // returns if the queue was empty in most cases,
   // it can give false positives when having high contention,
   // you can use it for e.g. to awake hybrid locks if you can tolerate some false positives
   bool push_back (Node* first, Node* last) {
      assert(last->UnsafeNext_ == nullptr);
      Node* beforeBack = this->Back_.exchange(last, std::memory_order_acquire);
      //(*) another push() here would succeed, a pop() when the queue size is > 1 would succeed too.
      beforeBack->AtomicNext_.store(first, std::memory_order_release);
      return beforeBack == &this->Stub_;
   }

   bool push_back (Node* node) {
      return this->push_back(node, node);
   }

   static constexpr Node* EmptyNode() { return nullptr; }
   static constexpr Node* BusyNode() { return EmptyNode() + 1; }

   /// \retval EmptyNode() 此時 slist 為空，無資料。
   /// \retval BusyNode()  有其他 thread 正在 push，無法完成 pop()，必須稍等一會兒。
   /// \retval else        有效的資料節點。
   Node* pop_front() {
      Node* first = this->Head_;
      Node* second = first->AtomicNext_.load(std::memory_order_acquire);
      if (first == &this->Stub_) {
         if (second == nullptr)
            return this->EmptyNode();
         // this->Head_ 從 this->Stub_ 移到真正的第一個 node.
         this->Head_ = second;
         first = second;
         second = second->AtomicNext_.load(std::memory_order_relaxed);
      }
      if (second) { // (當資料量大，最常發生到此)
         // 來到這兒，second 有可能是一個有效的 Node 或是 this->Stub_，不論是哪個，在此我們都不需要考慮 second。
         // 此時 first 必定是已經準備好可用的 Node.
      __FIRST_IS_DATA_NODE_AND_RETURN:
         // 可考慮在此取出一個 slist，而不只是一個 node。
         this->Head_ = second;
         first->UnsafeNext_ = nullptr;
         return first;
      }
      // 到此，因 second == nullptr，故 first 為最後一個 Node，但有可能其他 thread 正在執行 push_back() 尚未完成。
      // (completely commited pushes <= 1 from here and below) first_node.next = second_node = nullptr
      if (first != this->Back_.load(std::memory_order_relaxed)) {
         // 如果此時 first 不是 this->Back_ 表示其他 thread 正在執行 push_back() 尚未完成。
         // if the first node isn't the tail the memory snapshot that we have shows that someone is pushing just now.
         // The snapshot comes from the moment in time marked with an asterisk in a comment line in the push() function.
         // We have unconsistent data and we need to wait.
         return this->BusyNode();
      }
      // insert the stub node, so we can detect contention with other pushes.
      this->Stub_.UnsafeNext_ = nullptr;
      this->push_back(&this->Stub_);
      // remember that "push" had a full fence, the view now is consistent.
      second = first->UnsafeNext_;
      if (second) { // (當資料量小，偶爾 push 一個節點，則會走到此處)
         // "first.next" should point to either the stub node or something new that was completely pushed in between.
         goto __FIRST_IS_DATA_NODE_AND_RETURN;
      }
      // 來到這兒是有可能的: 當最先的 ThrA 執行 push 到(*)的地方被中斷，
      // (this->Back_ 已經換成 ThrA 的 last，但 beforeBack->Next_ 仍為 nullptr)，
      // 此時後續的 push，包含剛剛的 this->push_back(&this->Stub_)，都會加入到 ThrA 的 last 之後，
      // 所以此時的狀態為 busy，必須等 ThrA 完成。
      // we see the nullptr in next from a push that came before ours and the stub node is inserted after this new node.
      // this new push that came is incomplete (at the asterisk (*) point)
      return this->BusyNode();
   }

private:
   static const size_t          cache_line_size = 64;
   typedef char cacheline_pad_t[cache_line_size];
   cacheline_pad_t      CachePad0_;
   Node                 Stub_;
   cacheline_pad_t      CachePad1_;
   Node*                Head_;
   cacheline_pad_t      CachePad2_;
   std::atomic<Node*>   Back_;
   cacheline_pad_t      CachePad3_;
};

} // namespace
#endif//__fon9_mpsc_slist_hpp__
