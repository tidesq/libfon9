/// \file fon9/Trie.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_Trie_hpp__
#define __fon9_Trie_hpp__
#include "fon9/DyObj.hpp"
#include "fon9/StrTools.hpp"
#include <array>
#include <memory>
#include <algorithm>

namespace fon9 {

template <class LvKeyT>
class TrieKeyT {
   const LvKeyT* Begin_;
   const LvKeyT* End_;
public:
   template <size_t arysz>
   TrieKeyT(const LvKeyT(&ary)[arysz]) : Begin_{ary}, End_{ary + arysz} {}
   TrieKeyT(const LvKeyT* ibeg, const LvKeyT* iend) : Begin_{ibeg}, End_{iend} {}
   TrieKeyT(const LvKeyT* ibeg, size_t sz) : Begin_{ibeg}, End_{ibeg + sz} {}

   const LvKeyT* begin() const { return this->Begin_; }
   const LvKeyT* end() const { return this->End_; }
};

/// \ingroup Misc
/// 使用 byte array 當作 key.
struct TrieKeyByte {
   using LvKeyT = byte;
   using LvIndexT = byte;
   using key_type = TrieKeyT<LvKeyT>;
   static constexpr LvIndexT MaxIndex() {
      return 0xff;
   }
   static constexpr LvIndexT ToIndex(LvKeyT val) {
      return val;
   }
   static constexpr LvKeyT ToKey(LvIndexT idx) {
      return static_cast<LvKeyT>(idx);
   }
};

/// \ingroup Misc
/// 允許任意字元(0x00..0xff)當作 key.
/// - 例如: key_type = StrView: "abc" 則使用 3 bytes, 不含 EOS.
struct TrieKeyChar {
   using LvKeyT = char;
   using LvIndexT = byte;
   using key_type = StrView;
   static constexpr LvIndexT MaxIndex() {
      return 0xff;
   }
   static LvIndexT ToIndex(LvKeyT val) {
      return static_cast<LvIndexT>(val);
   }
   static LvKeyT ToKey(LvIndexT idx) {
      return static_cast<LvKeyT>(idx);
   }
};

/// \ingroup Misc
/// 僅允許 7bits printable[0x20..0x7e] ASCII 字元當作 key.
/// - 超過範圍的字元一律視為 0x7f.
/// - 例如: key_type = StrView: "abc" 則使用 3 bytes, 不含 EOS.
struct TrieKeyChPr : public TrieKeyChar {
   static constexpr LvIndexT MaxIndex() {
      return 0x7f - 0x20;
   }
   static LvIndexT ToIndex(LvKeyT val) {
      LvIndexT i = static_cast<LvIndexT>(val - 0x20);
      return i > MaxIndex() ? MaxIndex() : i;
   }
   static LvKeyT ToKey(LvIndexT idx) {
      return static_cast<LvKeyT>(idx + 0x20);
   }
};

/// \ingroup Misc
/// 僅允許 ASCII: 0..9, A..Z, a..z 當作 key.
/// 不考慮超過範圍的字元(例如: '&', '@'...), 您如果能保證「不可能」有超過範圍的字元, 則可直接使用.
/// 可是一旦出現超過範圍的字元, 則不保證結果, 極有可能會造成 crash!
struct TrieKeyAlNumNoBound {
   using LvKeyT = char;
   using LvIndexT = byte;
   using key_type = StrView;
   static constexpr LvIndexT MaxIndex() {
      return static_cast<LvIndexT>(kSeq2AlphaSize - 1);
   }
   static LvIndexT ToIndex(LvKeyT val) {
      return static_cast<LvIndexT>(Alpha2Seq(val));
   }
   static LvKeyT ToKey(LvIndexT idx) {
      return Seq2Alpha(idx);
   }
};

/// \ingroup Misc
/// 僅允許 ASCII: 0..9, A..Z, a..z 當作 key.
/// 如果有超過範圍的字元(例如: '&', '@'...),
/// 則視為相同, 放在該層的最後. 例如: "A12@4" == "A12&4".
/// 無效字元 ToKey() 一律傳回 '?'
struct TrieKeyAlNum : public TrieKeyAlNumNoBound {
   static constexpr LvIndexT MaxIndex() {
      return static_cast<LvIndexT>(kSeq2AlphaSize);
   }
   static LvIndexT ToIndex(LvKeyT val) {
      LvIndexT i = static_cast<LvIndexT>(Alpha2Seq(val));
      return(i > MaxIndex() ? MaxIndex() : i);
   }
   static LvKeyT ToKey(LvIndexT idx) {
      return idx >= kSeq2AlphaSize ? '?' : Seq2Alpha(idx);
   }
};

fon9_WARN_DISABLE_PADDING;
/// \ingroup Misc
/// 使用 level 的方式替「不定長度的Key」建立對照表.
/// 如果是固定長度(e.g. uint32_t, uint64_t...) 則建議使用 LevelArray<>
template <class KeyTransT, class ValueT>
class Trie {
   fon9_NON_COPYABLE(Trie);
   using LvKeyT = typename KeyTransT::LvKeyT;
   using LvIndexT = typename KeyTransT::LvIndexT;
   struct Node;
   using NodeObj = DyObj<Node>;
   using ValueObj = DyObj<ValueT>;
   struct LvAry {
      fon9_NON_COPY_NON_MOVE(LvAry);
      using Ary = std::array<NodeObj, KeyTransT::MaxIndex() + 1u>;
      Ary      Ary_;
      Node&    OwnerNode_;
      /// 包含 this level(this->Ary_) & 下一層全部的物件數量.
      size_t   Count_{0};
      LvIndexT IndexMin_;
      LvIndexT IndexMax_;
      LvAry(Node& owner, LvIndexT idx) : OwnerNode_(owner), IndexMin_{idx}, IndexMax_{idx} {
         this->Ary_[idx].emplace(this);
      }
      LvIndexT GetNodeIndex(const Node& node) const {
         LvIndexT idx = static_cast<LvIndexT>(&NodeObj::StaticCast(node) - this->Ary_.data());
         assert(idx < this->Ary_.size());
         return idx;
      }
      LvKeyT GetNodeKey(const Node& node) const {
         return KeyTransT::ToKey(GetNodeIndex(node));
      }
      Node* First();
      Node* Next(const Node& cur) {
         return this->Next(this->GetNodeIndex(cur));
      }
      Node* Next(LvIndexT idx);
      Node* Last();
      Node* Prev(const Node& cur);
      static void DecCount(LvAry*);
      static void IncCount(LvAry*);
   };
   struct Node {
      fon9_NON_COPY_NON_MOVE(Node);
      LvAry*                  OwnerAry_;
      std::unique_ptr<LvAry>  NextLevel_;
      ValueObj                Value_;

      Node(LvAry* owner) : OwnerAry_{owner} {
      }
      ~Node() {
         this->Clear();
      }
      size_t Clear() {
         size_t count = this->Value_.clear();
         if (LvAry* lvNext = this->NextLevel_.get()) {
            count += lvNext->Count_;
            this->NextLevel_.reset();
         }
         return count;
      }
      Node* Last() {
         if (this->NextLevel_)
            return this->NextLevel_->Last();
         assert(this->Value_.get());
         return this;
      }
      Node* Prev() const {
         if (this->OwnerAry_)
            return this->OwnerAry_->Prev(*this);
         return nullptr;
      }

      Node* First() {
         if (this->Value_.get())
            return this;
         assert(this->NextLevel_);
         return this->NextLevel_->First();
      }
      Node* Next() const {
         if (this->NextLevel_)
            return this->NextLevel_->First();
         return this->OwnerNext();
      }
      Node* OwnerNext() const {
         if (this->OwnerAry_)
            return this->OwnerAry_->Next(*this);
         return nullptr;
      }
   };

   enum SearchFn {
      SearchFn_LowerBound = -1,
      SearchFn_UpperBound = 1,
   };
   static Node* SearchBound(Node* cur, const typename KeyTransT::key_type& key, SearchFn sfn) {
      if (fon9_UNLIKELY(cur == nullptr))
         return nullptr;
      for (LvKeyT k : key) {
         if (LvAry* lv = cur->NextLevel_.get()) {
            LvIndexT idx = KeyTransT::ToIndex(k);
            if ((cur = lv->Ary_[idx].get()) != nullptr)
               continue;
            if (idx < lv->IndexMin_)
               cur = lv->Ary_[lv->IndexMin_].get();
            else
               return lv->Next(idx);
         }
         return cur->First();
      }
      if (cur->Value_.get())
         return(sfn == SearchFn_UpperBound ? cur->Next() : cur);
      return cur->First();
   }

   Node* find_node(const typename KeyTransT::key_type& key) {
      if (Node* cur = this->Head_.get()) {
         for (LvKeyT k : key) {
            if (LvAry* lv = cur->NextLevel_.get()) {
               if ((cur = lv->Ary_[KeyTransT::ToIndex(k)].get()) != nullptr)
                  continue;
            }
            return nullptr;
         }
         return cur;
      }
      return nullptr;
   }

   std::unique_ptr<Node> Head_;
public:
   Trie() = default;
   Trie(Trie&&) = default;
   Trie& operator=(Trie&& rhs) = default;

   void swap(Trie& rhs) {
      this->Head_.swap(rhs.Head_);
   }
   size_t size() const {
      if (this->Head_ == nullptr)
         return 0;
      if (this->Head_->NextLevel_)
         return this->Head_->NextLevel_->Count_ + (this->Head_->Value_.get() != nullptr);
      return (this->Head_->Value_.get() ? 1u : 0u);
   }
   bool empty() const {
      return this->Head_ == nullptr;
   }
   void clear() {
      Trie autodel{std::move(*this)};
   }

   using mapped_type = ValueT;
   using key_type = typename KeyTransT::key_type;
   using keystr_type = std::basic_string<LvKeyT>;
   class value_type {
      Node* Node_;
      friend class Trie;
   public:
      value_type(Node* node) : Node_{node} {
      }
      const mapped_type& value() const {
         return *this->Node_->Value_.get();
      }
      mapped_type& value() {
         return *this->Node_->Value_.get();
      }
      keystr_type key() const {
         keystr_type keystr;
         if (Node* cur = this->Node_)
            while (LvAry* lv = cur->OwnerAry_) {
               keystr.push_back(lv->GetNodeKey(*cur));
               cur = &lv->OwnerNode_;
            }
         std::reverse(keystr.begin(), keystr.end());
         return keystr;
      }
   };
   struct IteratorControl {
      Node*                Head_;
      value_type  mutable  Cur_;
      IteratorControl(Node* head, Node* cur) : Head_{head}, Cur_{cur} {
      }
      void ToNext() {
         assert(this->Cur_.Node_);
         if (this->Cur_.Node_)
            this->Cur_.Node_ = this->Cur_.Node_->Next();
      }
      void ToPrev() {
         assert(this->Head_);
         if (this->Cur_.Node_)
            this->Cur_.Node_ = this->Cur_.Node_->Prev();
         else
            this->Cur_.Node_ = this->Head_->Last();
      }
   };

   class const_iterator;
   template <class VType>
   class iterator_base : protected IteratorControl {
      friend class Trie;
      friend class const_iterator;
   public:
      using IteratorControl::IteratorControl;
      iterator_base(const IteratorControl& rhs) : IteratorControl(rhs) {}
      iterator_base(const iterator_base&) = default;
      iterator_base() : iterator_base{nullptr, nullptr} {}

      bool operator==(const iterator_base& rhs) const { return this->Cur_.Node_ == rhs.Cur_.Node_ && this->Head_ == rhs.Head_; }
      bool operator!=(const iterator_base& rhs) const { return !operator==(rhs); }
      VType& operator*() const { return this->Cur_; }
      VType* operator->() const { return &this->Cur_; }
      iterator_base operator++(int) {
         iterator_base i = *this;
         ++(*this);
         return i;
      }
      iterator_base& operator++() {
         this->ToNext();
         return *this;
      }
      iterator_base operator--(int) {
         iterator_base i = *this;
         --(*this);
         return i;
      }
      iterator_base& operator--() {
         this->ToPrev();
         return *this;
      }
   };

   using iterator = iterator_base<value_type>;
   iterator begin() {
      if (fon9_LIKELY(this->Head_))
         return iterator{this->Head_.get(), this->Head_->First()};
      return iterator(nullptr, nullptr);
   }
   iterator end() { return iterator{this->Head_.get(), nullptr}; }

   mapped_type* find_mapped(const key_type& key) {
      if (Node* cur = this->find_node(key))
         return cur->Value_.get();
      return nullptr;
   }
   const mapped_type* find_mapped(const key_type& key) const {
      return const_cast<Trie*>(this)->fond_mapped(key);
   }

   iterator find(const key_type& key) {
      if (Node* cur = this->find_node(key)) {
         if (cur->Value_.get())
            return iterator{this->Head_.get(), cur};
      }
      return this->end();
   }
   iterator lower_bound(const key_type& key) {
      return iterator{this->Head_.get(), SearchBound(this->Head_.get(), key, SearchFn_LowerBound)};
   }
   iterator upper_bound(const key_type& key) {
      return iterator{this->Head_.get(), SearchBound(this->Head_.get(), key, SearchFn_UpperBound)};
   }

   template <class... ArgsT>
   std::pair<iterator, bool> emplace(const key_type& key, ArgsT&&... args) {
      Node* cur = this->Head_.get();
      if (fon9_UNLIKELY(cur == nullptr))
         this->Head_.reset(cur = new Node(nullptr));
      for (LvKeyT k : key) {
         LvIndexT idx = KeyTransT::ToIndex(k);
         LvAry*   lv = cur->NextLevel_.get();
         if (lv == nullptr) {
            cur->NextLevel_.reset(lv = new LvAry(*cur, idx));
            cur = lv->Ary_[idx].get();
            continue;
         }
         if (idx < lv->IndexMin_)
            lv->IndexMin_ = idx;
         else if (lv->IndexMax_ < idx)
            lv->IndexMax_ = idx;
         if ((cur = lv->Ary_[idx].get()) == nullptr)
            cur = lv->Ary_[idx].emplace(lv);
      }
      if (cur->Value_.get())
         return std::make_pair(iterator{this->Head_.get(), cur}, false);
      LvAry::IncCount(cur->OwnerAry_);
      cur->Value_.emplace(std::forward<ArgsT>(args)...);
      return std::make_pair(iterator{this->Head_.get(), cur}, true);
   }

   /// 移除之後, 如果 this->empty()==true; 則之前取得的 this->end(); 會失效!
   iterator erase(iterator i) {
      assert(i.Head_ == this->Head_.get() && i.Cur_.Node_ != nullptr);
      Node* cur = i.Cur_.Node_;
      assert(cur->Value_.get());
      cur->Value_.clear();
      if (LvAry* lvNext = cur->NextLevel_.get()) {
         LvAry::DecCount(cur->OwnerAry_);
         return iterator{this->Head_.get(), lvNext->First()};
      }
      while(LvAry* ownerAry = cur->OwnerAry_) {
         LvIndexT idx = ownerAry->GetNodeIndex(*cur);
         if (ownerAry->IndexMin_ != ownerAry->IndexMax_) {
            ownerAry->Ary_[idx].clear();
            if (idx == ownerAry->IndexMin_) {
               while (ownerAry->Ary_[++ownerAry->IndexMin_].get() == nullptr
                      && ownerAry->IndexMin_ < ownerAry->IndexMax_) {
               }
               cur = ownerAry->First();
            }
            else if (idx == ownerAry->IndexMax_) {
               while (ownerAry->Ary_[--ownerAry->IndexMax_].get() == nullptr
                      && ownerAry->IndexMin_ < ownerAry->IndexMax_) {
               }
               cur = ownerAry->OwnerNode_.OwnerNext();
            }
            else {
               cur = ownerAry->Next(idx);
            }
            LvAry::DecCount(ownerAry);
            return iterator{this->Head_.get(), cur};
         }
         assert(idx == ownerAry->IndexMin_);
         cur = &ownerAry->OwnerNode_;
         if (cur->Value_.get()) {
            assert(cur->NextLevel_.get() == ownerAry);
            cur->NextLevel_.reset();
            LvAry::DecCount(cur->OwnerAry_);
            return iterator(this->Head_.get(), cur->OwnerNext());
         }
      }
      assert(this->Head_.get() == cur);
      this->Head_.reset();
      return this->end();
   }

   class const_iterator : public iterator_base<const value_type> {
      using base = iterator_base<const value_type>;
   public:
      using base::base;
      const_iterator(const iterator& i) : base{*static_cast<const IteratorControl*>(&i)} {
      }
      const_iterator& operator=(const iterator& i) {
         *static_cast<IteratorControl*>(this) = i;
         return *this;
      }
   };
   const_iterator cbegin() const { return const_iterator{const_cast<Trie*>(this)->begin()}; }
   const_iterator cend() const { return const_iterator{this->Head_.get(), nullptr}; }
   const_iterator begin() const { return this->cbegin(); }
   const_iterator end() const { return this->cend(); }

   const_iterator find(const key_type& key) const { return const_iterator{const_cast<Trie*>(this)->find(key)}; }
   const_iterator lower_bound(const key_type& key) const { return const_iterator{const_cast<Trie*>(this)->lower_bound(key)}; }
   const_iterator upper_bound(const key_type& key) const { return const_iterator{const_cast<Trie*>(this)->upper_bound(key)}; }

   /// 尋找指定開頭key的最後一筆資料, 例:
   /// - keyHead = "A"; 找到 "A" 下層的最後序號, 例: "A1234";
   ///   如果第一層沒有 "A", 則傳回 empty();
   /// - keyHead = "AB"; 找到 "AB" 下層的最後序號, 例: "AB123";
   ///   如果第一層有 "A", 但下一層沒有 "B", 則傳回 "A";
   /// - KeyStr out 必須支援 KeyStr::clear(); KeyStr::push_back(k);
   template <class KeyStr>
   void find_tail_keystr(const key_type& keyHead, KeyStr& out) const {
      out.clear();
      if (Node* cur = this->Head_.get()) {
         for (LvKeyT k : keyHead) {
            if (LvAry* lv = cur->NextLevel_.get()) {
               if ((cur = lv->Ary_[KeyTransT::ToIndex(k)].get()) != nullptr) {
                  out.push_back(k);
                  continue;
               }
            }
            return;
         }
         while (LvAry* lv = cur->NextLevel_.get()) {
            out.push_back(KeyTransT::ToKey(lv->IndexMax_));
            cur = lv->Ary_[lv->IndexMax_].get();
         }
      }
   }
};
fon9_WARN_POP;

template <class KeyTransT, class ValueT>
typename Trie<KeyTransT, ValueT>::Node*
Trie<KeyTransT, ValueT>::LvAry::First() {
   Node* node = this->Ary_[this->IndexMin_].get();
   assert(node != nullptr);
   return node->First();
}

template <class KeyTransT, class ValueT>
typename Trie<KeyTransT, ValueT>::Node*
Trie<KeyTransT, ValueT>::LvAry::Next(LvIndexT idx) {
   while (idx < this->IndexMax_) {
      if (Node* node = this->Ary_[++idx].get())
         return node->First();
   }
   return this->OwnerNode_.OwnerNext();
}

template <class KeyTransT, class ValueT>
typename Trie<KeyTransT, ValueT>::Node*
Trie<KeyTransT, ValueT>::LvAry::Last() {
   Node* node = this->Ary_[this->IndexMax_].get();
   assert(node != nullptr);
   return node->Last();
}

template <class KeyTransT, class ValueT>
typename Trie<KeyTransT, ValueT>::Node*
Trie<KeyTransT, ValueT>::LvAry::Prev(const Node& cur) {
   LvIndexT idx = this->GetNodeIndex(cur);
   while (idx > this->IndexMin_) {
      if (Node* node = this->Ary_[--idx].get())
         return node->Last();
   }
   if (this->OwnerNode_.Value_.get())
      return &this->OwnerNode_;
   return this->OwnerNode_.Prev();
}

template <class KeyTransT, class ValueT>
void Trie<KeyTransT, ValueT>::LvAry::DecCount(LvAry* pary) {
   while(pary) {
      assert(pary->Count_ > 0);
      --pary->Count_;
      pary = pary->OwnerNode_.OwnerAry_;
   }
}

template <class KeyTransT, class ValueT>
void Trie<KeyTransT, ValueT>::LvAry::IncCount(LvAry* pary) {
   while (pary) {
      ++pary->Count_;
      pary = pary->OwnerNode_.OwnerAry_;
   }
}

} // namespace
#endif//__fon9_Trie_hpp__
