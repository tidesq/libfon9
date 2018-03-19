/// \file fon9/SinglyLinkedList.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_SinglyLinkedList_hpp__
#define __fon9_SinglyLinkedList_hpp__
#include "fon9/sys/Config.hpp"

namespace fon9 {

/// \ingroup Misc
/// - 僅提供在前端增加節點(push_front)的單向串鍊.
/// - Node 必須提供:
///   - FreeNode(Node*);       // 釋放(刪除)單一節點.
///   - Node* Node::GetNext();
///   - Node::SetNext(Node*);  // 若您要保護此 mf, 避免外界使用, 可考慮繼承 SinglyLinkedListNode<>
template <class Node>
class SinglyLinkedList {
   fon9_NON_COPYABLE(SinglyLinkedList);
   static void FreeList(Node* first) {
      while (Node* curr = first) {
         first = first->GetNext();
         FreeNode(curr);
      }
   }
   static size_t CalcNodeCount(Node* node) {
      size_t count = 0;
      while (node) {
         node = node->GetNext();
         ++count;
      }
      return count;
   }

   Node*    Head_{nullptr};
   size_t   Count_{0};

   void InternalClear() {
      this->Head_ = nullptr;
      this->Count_ = 0;
   }
   /// 給內部使用 no assert 的版本.
   SinglyLinkedList(size_t count, Node* head) : Head_{head}, Count_{count} {
   }

protected:
   /// 提供給 衍生者(SinglyLinkedList2) 在 this 尾端(thisBack)加入一個節點.
   void PushBack(Node** thisBack, Node* node) {
      assert(node->GetNext() == nullptr);
      if (fon9_LIKELY(*thisBack)) {
         (*thisBack)->SetNext(node);
         ++this->Count_;
      }
      else {
         assert(this->Head_ == nullptr);
         this->push_front(node);
      }
      (*thisBack) = node;
   }
   /// 提供給 衍生者(SinglyLinkedList2) 移入一個串鍊到 this 的尾端(thisBack).
   void PushBack(Node** thisBack, SinglyLinkedList&& src, Node** srcBack) {
      if (src.empty())
         return;
      this->Count_ += src.size();
      Node*  srcFront = src.ReleaseList();
      assert(srcFront);
      if (fon9_LIKELY(*thisBack))
         (*thisBack)->SetNext(srcFront);
      else {
         assert(this->Head_ == nullptr);
         this->Head_ = srcFront;
      }
      *thisBack = *srcBack;
      *srcBack = nullptr;
   }

public:
   ~SinglyLinkedList() {
      FreeList(this->Head_);
   }

   SinglyLinkedList() = default;
   /// 這是個危險的建構, 但是如果您確定, head & count 且不屬於其他 list, 則可以安全地使用.
   SinglyLinkedList(Node* head, size_t count) : Head_{head}, Count_{count} {
      assert(count == CalcNodeCount(head));
   }

   SinglyLinkedList(SinglyLinkedList&& r) : Head_{r.Head_}, Count_{r.Count_} {
      r.InternalClear();
   }
   SinglyLinkedList& operator=(SinglyLinkedList&& r) {
      SinglyLinkedList autoFree(std::move(*this));
      this->Head_ = r.Head_;
      this->Count_ = r.Count_;
      r.InternalClear();
      return *this;
   }

   /// 傳回整串 list 交給呼叫者管理, 此時 this 為空.
   Node* ReleaseList() {
      Node* head = this->Head_;
      this->InternalClear();
      return head;
   }

   /// 從串列頭移出一個節點.
   /// 若 this->empty() 則傳回 nullptr.
   Node* pop_front() {
      if (!this->Head_)
         return nullptr;
      --this->Count_;
      Node* head = this->Head_;
      this->Head_ = head->GetNext();
      head->SetNext(nullptr);
      return head;
   }

   /// 移出前 n 個節點.
   SinglyLinkedList pop_front(size_t n) {
      if (n <= 0)
         return SinglyLinkedList{};
      if (this->Count_ <= n)
         return SinglyLinkedList{std::move(*this)};
      this->Count_ -= n;
      Node* head = this->Head_;
      SinglyLinkedList retval{n, head};
      while (--n > 0)
         head = head->GetNext();
      this->Head_ = head->GetNext();
      head->SetNext(nullptr);
      return retval;
   }

   /// 保留前 n 個節點, 傳回第 n 個節點以後的串列.
   SinglyLinkedList split(size_t n) {
      if (n <= 0)
         return std::move(*this);
      if (this->Count_ <= n)
         return SinglyLinkedList{};
      SinglyLinkedList retval{this->Count_ - n, this->Head_};
      this->Count_ = n;
      while (--n > 0)
         retval.Head_ = retval.Head_->GetNext();
      Node* p = retval.Head_->GetNext();
      retval.Head_->SetNext(nullptr);
      retval.Head_ = p;
      return retval;
   }

   /// 在前方加入一個無主的 node.
   /// - 如果 node 在別的串列尾端, 則會有無法預期的結果!
   /// - node 不可為 nullptr, 且 node 沒有 next: node->GetNext()==nullptr.
   void push_front(Node* node) {
      assert(node != nullptr && node->GetNext() == nullptr);
      node->SetNext(this->Head_);
      this->Head_ = node;
      ++this->Count_;
   }

   /// 把 rhs 移動到 this 的前端, 結束後 rhs 會被清空. 例:
   /// - before: this=A,B,C; rhs=1,2,3;
   /// - after: this=1,2,3,A,B,C; rhs=empty;
   void push_front(SinglyLinkedList&& rhs) {
      if (Node* rtail = rhs.Head_) {
         if (this->Head_) {
            while (Node* next = rtail->GetNext())
               rtail = next;
            rtail->SetNext(this->Head_);
         }
         this->Head_ = rhs.Head_;
         this->Count_ += rhs.Count_;
         rhs.InternalClear();
      }
   }
   /// 若 this->empty() 則傳回 nullptr.
   Node* front() {
      return this->Head_;
   }
   const Node* front() const {
      return this->Head_;
   }
   const Node* cfront() const {
      return this->Head_;
   }

   bool empty() const {
      return this->Head_ == nullptr;
   }
   /// 在 push, pop 時會維護節點數量, 所以這裡可以立即取得數量.
   size_t size() const {
      return this->Count_;
   }
};

/// \ingroup Misc
/// 除了 SinglyLinkedList 全部的功能外, 提供快速在尾端增加節點(push_back)的單向串鍊.
template <class Node>
class SinglyLinkedList2 : private SinglyLinkedList<Node> {
   fon9_NON_COPYABLE(SinglyLinkedList2);
   using base = SinglyLinkedList<Node>;
   Node*    Back_{};
public:
   SinglyLinkedList2() = default;
   SinglyLinkedList2(SinglyLinkedList2&& r) : base{std::move(r)}, Back_{r.Back_} {
      r.Back_ = nullptr;
   }
   SinglyLinkedList2& operator=(SinglyLinkedList2&& r) {
      base  autoFree(std::move(*this));
      *static_cast<base*>(this) = static_cast<base&&>(r);
      this->Back_ = r.Back_;
      r.Back_ = nullptr;
      return *this;
   }

   Node* pop_front() {
      if (Node* node = base::pop_front()) {
         if (this->Back_ == node)
            this->Back_ = nullptr;
         return node;
      }
      return nullptr;
   }

   void push_front(Node* node) {
      base::push_front(node);
      if (!this->Back_)
         this->Back_ = node;
   }

   void push_back(Node* node) {
      this->PushBack(&this->Back_, node);
   }

   /// 把 rhs 移動到 this 的尾端, 結束後 rhs 會被清空. 例:
   /// - before: this=A,B,C; rhs=1,2,3;
   /// - after: this=A,B,C,1,2,3; rhs=empty;
   void push_back(SinglyLinkedList2&& rhs) {
      this->PushBack(&this->Back_, std::move(rhs), &rhs.Back_);
   }

   void push_front(SinglyLinkedList2&& rhs) {
      if (fon9_LIKELY(!rhs.empty())) {
         rhs.move_to_back(std::move(*this));
         *this = std::move(rhs);
      }
   }

   Node* back() {
      return this->Back_;
   }
   using base::front;
   using base::cfront;
   using base::empty;
   using base::size;
};

/// \ingroup Misc
/// 單向串列節點的基底.
/// \code
///   struct MyNode : public SinglyLinkedListNode<MyNode> {
///   };
/// \endcode
template <class DerivedT>
class SinglyLinkedListNode {
   fon9_NON_COPY_NON_MOVE(SinglyLinkedListNode);
   DerivedT* Next_{};

   friend class SinglyLinkedList<DerivedT>;
   void SetNext(DerivedT* next) { this->Next_ = next; }
public:
   SinglyLinkedListNode() = default;
   DerivedT* GetNext() const { return this->Next_; }
};

} // namespace
#endif//__fon9_SinglyLinkedList_hpp__
