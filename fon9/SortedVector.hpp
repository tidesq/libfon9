/// \file fon9/SortedVector.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_SortedVector_hpp__
#define __fon9_SortedVector_hpp__
#include "fon9/sys/Config.hpp"
fon9_BEFORE_INCLUDE_STD;
#include <vector>
#include <algorithm>
fon9_AFTER_INCLUDE_STD;

namespace fon9  {

fon9_WARN_DISABLE_PADDING;
/// \ingroup Misc
/// 資料有排序的陣列.
/// - 因所有 methods 都相似於 STL, 所以命名方式同 STL.
/// - value_type = std::pair<const K, V>
template <class K, class V, class Compare = std::less<K>>
class SortedVector {
   using DeconstK = typename std::remove_const<K>::type;
   using InternalContainer = std::vector<std::pair<DeconstK, V>>;
   using PublicContainer = std::vector<std::pair<const DeconstK, V>>;
   InternalContainer Container_;
   Compare           Cmp_;
   PublicContainer* Container() { return reinterpret_cast<PublicContainer*>(&this->Container_); }
   const PublicContainer* ConstContainer() const { return reinterpret_cast<const PublicContainer*>(&this->Container_); }
   typename PublicContainer::iterator ConvIterator(typename InternalContainer::const_iterator i) {
      return this->Container()->begin() + (i - this->Container_.cbegin());
   }
   typename InternalContainer::iterator ConvIterator(typename PublicContainer::const_iterator i) {
      return this->Container_.begin() + (i - this->Container()->cbegin());
   }
protected:
   template <class I, class KeyT>
   static I lower_i(I first, I last, const KeyT& key, const Compare& cmp) {
      return std::lower_bound(first, last, key, [&cmp](const value_type& i, const KeyT& k) -> bool {
         return cmp(*const_cast<const K*>(&i.first), k);
      });
   }
   template <class I, class KeyT>
   static bool find_i(I& res, I first, I last, const KeyT& key, const Compare& cmp) {
      res = lower_i(first, last, key, cmp);
      return res != last && !cmp(key, *const_cast<const K*>(&res->first));
   }
   template <class I, class KeyT>
   static I find_i(I first, I last, const KeyT& key, const Compare& cmp) {
      I i;
      return find_i(i, first, last, key, cmp) ? i : last;
   }
   /// 您必須自行確定: 新加入尾端的元素, key 值必須正確!
   template <class... ArgsT>
   void emplace_back(ArgsT&&... args) {
      this->Container()->emplace_back(std::forward<ArgsT>(args)...);
   }
public:
   using iterator = typename PublicContainer::iterator;
   using const_iterator = typename PublicContainer::const_iterator;
   using size_type = typename PublicContainer::size_type;
   using difference_type = typename PublicContainer::difference_type;
   using reference = typename PublicContainer::reference;
   using const_reference = typename PublicContainer::const_reference;
   using value_type = typename PublicContainer::value_type;
   using key_type = K;
   using mapped_type = V;
   using key_compare = Compare;

   SortedVector(const Compare& c = Compare{}) : Container_{}, Cmp_(c) {
   }
   /// 建構時加入元素[first..last)可以沒有排序, 返回前會將加入的元素進行排序.
   template <class InputIterator>
   SortedVector(InputIterator first, InputIterator last, const Compare& c = Compare{})
      : Container_{first, last}
      , Cmp_(c) {
      std::sort(this->Container_.begin(), this->Container_.end(), this->Cmp_);
   }
   void swap(SortedVector& r) { this->Container_.swap(r.Container_); }
   key_compare key_comp() const { return this->Cmp_; }

   iterator begin() { return this->Container()->begin(); }
   const_iterator begin() const { return this->ConstContainer()->begin(); }
   const_iterator cbegin() const { return this->ConstContainer()->begin(); }
   iterator end() { return this->Container()->end(); }
   const_iterator end() const { return this->ConstContainer()->end(); }
   const_iterator cend() const { return this->ConstContainer()->end(); }

   bool empty() const { return this->Container_.empty(); }
   size_type size() const { return this->Container_.size(); }
   void reserve(size_type new_cap) { this->Container_.reserve(new_cap); }
   void shrink_to_fit() { this->Container_.shrink_to_fit(); }
   void clear() { this->Container_.clear(); }

   // 不提供 operator[](xx); 因為 [xx] = [vector index] or [key]? 會造成混淆!
   // reference operator[](size_type pos) { return this->Container()->operator[](pos); }
   // const_reference operator[](size_type pos) const { return this->ConstContainer()->operator[](pos); }
   reference vindex(size_type pos) { return this->Container()->operator[](pos); }
   const_reference vindex(size_type pos) const { return this->ConstContainer()->operator[](pos); }

   template <class KeyT>
   reference kindex(const KeyT& key) {
      iterator ifind = this->find(key);
      if (ifind != this->end())
         return *ifind;
      return *this->insert(value_type{key, mapped_type{}}).first;
   }

   reference back() { return this->Container()->back(); }
   const_reference back() const { return this->ConstContainer()->back(); }
   void pop_back() { this->Container_.pop_back(); }

   /// 移除 pred(value_type&) 傳回 true 的資料.
   /// \return 傳回移除的數量.
   template <class Pred>
   size_type remove_if(size_type first, size_type last, Pred pred) {
      if (first >= last)
         return 0;
      auto ifrom = this->Container_.begin() + static_cast<difference_type>(first);
      auto ito = this->Container_.begin() + static_cast<difference_type>(last);
      auto itail = std::remove_if(ifrom, ito, [&pred](typename InternalContainer::value_type& v)->bool {
                              return pred(*reinterpret_cast<value_type*>(&v));
                           });
      difference_type count = ito - itail;
      if (count <= 0)
         return 0;
      this->Container_.erase(itail, ito);
      return static_cast<size_type>(count);
   }
   iterator erase(const_iterator pos) { return ConvIterator(this->Container_.erase(ConvIterator(pos))); }
   iterator erase(const_iterator first, const_iterator last) { return ConvIterator(this->Container_.erase(ConvIterator(first), ConvIterator(last))); }

   /// 依照排序加入一個元素.
   /// vtype 可為 const value_type& 或 value_type&&
   template <class vtype>
   std::pair<iterator,bool> insert(vtype&& v) {
      iterator i;
      if (find_i(i, this->begin(), this->end(), v.first, this->Cmp_))
         return std::make_pair(i, false);
      return std::make_pair(ConvIterator(this->Container_.insert(ConvIterator(i), std::forward<vtype>(v))), true);
   }
   /// 依照排序加入一個元素.
   /// vtype 可為 const value_type& 或 value_type&&
   template <class vtype>
   iterator insert(iterator ihint, vtype&& v) {
      iterator iend = this->end();
      iterator ibeg = this->begin();
      if (fon9_UNLIKELY(ihint == iend)) {
         if (fon9_UNLIKELY(iend == ibeg)
         || fon9_LIKELY(this->Cmp_((ihint - 1)->first, v.first))) {
            this->Container_.emplace_back(std::forward<vtype>(v));
            return ConvIterator(this->Container_.end() - 1);
         }
         ibeg = this->begin();
      }
      else if (fon9_LIKELY(this->Cmp_(v.first, ihint->first))) {
         if (fon9_UNLIKELY(ihint == ibeg)
         || fon9_LIKELY(this->Cmp_((ihint - 1)->first, v.first)))
            return ConvIterator(this->Container_.insert(ConvIterator(ihint), std::forward<vtype>(v)));
         iend = ihint;
      }
      else if (ibeg != ihint) {
         --ibeg;
      }
      iterator ifind;
      if (find_i(ifind, ibeg, iend, v.first, this->Cmp_))
         return ifind;
      return ConvIterator(this->Container_.insert(ConvIterator(ifind), std::forward<vtype>(v)));
   }

   template <class KeyT>
   iterator       find(const KeyT& key)       { return find_i(this->begin(), this->end(), key, this->Cmp_); }
   template <class KeyT>
   const_iterator find(const KeyT& key) const { return find_i(this->begin(), this->end(), key, this->Cmp_); }

   template <class KeyT>
   iterator       lower_bound(const KeyT& key)       { return lower_i(this->begin(), this->end(), key, this->Cmp_); }
   template <class KeyT>
   const_iterator lower_bound(const KeyT& key) const { return lower_i(this->begin(), this->end(), key, this->Cmp_); }

   template <class RMap>
   bool is_equal(const RMap& rhs) const {
      if (this->size() != rhs.size())
         return false;
      const_iterator i = this->begin();
      for (const value_type& v : rhs) {
         if (i->first == v.first && i->second == v.second)
            ++i;
         else
            return false;
      }
      return true;
   }
};

/// \ingroup Misc
/// 資料有排序的陣列集合.
/// - 因所有 methods 都相似於 STL, 所以命名方式同 STL.
/// - value_type = V
/// - V 裡面的 key 不能變動: 由使用者自行保護.
/// - Compare 必須提供 (key_type 由使用者自行決定).
///   - bool Compare::operator()(const V& v, const key_type& key);
///   - bool Compare::operator()(const key_type& key, const V& v);
///   - bool Compare::operator()(const V& lhs, const V& rhs);
template <class V, class Compare = std::less<V>>
class SortedVectorSet {
   using Container = std::vector<V>;
   Container   Container_;
   Compare     Cmp_;
protected:
   template <class Iterator, class KeyT>
   static Iterator lower_i(Iterator first, Iterator last, const KeyT& key, const Compare& cmp) {
      return std::lower_bound(first, last, key, [&cmp](const V& v, const KeyT& k) -> bool {
         return cmp(v, k);
      });
   }
   template <class Iterator, class KeyT>
   static bool find_i(Iterator& res, Iterator first, Iterator last, const KeyT& key, const Compare& cmp) {
      res = lower_i(first, last, key, cmp);
      return res != last && !cmp(key, *res);
   }
   template <class Iterator, class KeyT>
   static Iterator find_i(Iterator first, Iterator last, const KeyT& key, const Compare& cmp) {
      Iterator i;
      return find_i(i, first, last, key, cmp) ? i : last;
   }
   /// 您必須自行確定: 新加入尾端的元素, key 值必須正確!
   template <class... ArgsT>
   void emplace_back(ArgsT&&... args) {
      this->Container()->emplace_back(std::forward<ArgsT>(args)...);
   }
public:
   using iterator = typename Container::iterator;
   using const_iterator = typename Container::const_iterator;
   using size_type = typename Container::size_type;
   using difference_type = typename Container::difference_type;
   using reference = typename Container::reference;
   using const_reference = typename Container::const_reference;
   using value_type = V;
   using key_compare = Compare;
   using value_compare = Compare;

   SortedVectorSet(const Compare& c = Compare{}) : Container_{}, Cmp_(c) {
   }
   /// 建構時加入元素[first..last)可以沒有排序, 返回前會將加入的元素進行排序.
   template <class InputIterator>
   SortedVectorSet(InputIterator first, InputIterator last, const Compare& c = Compare{})
      : Container_{first, last}
      , Cmp_(c) {
      std::sort(this->Container_.begin(), this->Container_.end(), this->Cmp_);
   }
   void swap(SortedVectorSet& r) { this->Container_.swap(r.Container_); }
   key_compare key_comp() const { return this->Cmp_; }

   iterator begin() { return this->Container_.begin(); }
   const_iterator begin() const { return this->Container_.begin(); }
   const_iterator cbegin() const { return this->Container_.begin(); }
   iterator end() { return this->Container_.end(); }
   const_iterator end() const { return this->Container_.end(); }
   const_iterator cend() const { return this->Container_.end(); }

   bool empty() const { return this->Container_.empty(); }
   size_type size() const { return this->Container_.size(); }
   void reserve(size_type new_cap) { this->Container_.reserve(new_cap); }
   void shrink_to_fit() { this->Container_.shrink_to_fit(); }
   void clear() { this->Container_.clear(); }

   // 不提供 operator[](xx); 因為 [xx] = [vector index] or [key]? 會造成混淆!
   // reference operator[](size_type pos) { return this->Container_.operator[](pos); }
   reference vindex(size_type pos) { return this->Container_.operator[](pos); }
   const_reference vindex(size_type pos) const { return this->Container_.operator[](pos); }

   /// 必須提供 value_type{key} 建構.
   template <class KeyT>
   reference kindex(const KeyT& key) {
      iterator ifind = this->find(key);
      if (ifind != this->end())
         return *ifind;
      return *this->insert(value_type{key}).first;
   }

   reference back() { return this->Container_.back(); }
   const_reference back() const { return this->Container_.back(); }
   void pop_back() { this->Container_.pop_back(); }

   iterator erase(const_iterator pos) { return this->Container_.erase(pos); }
   iterator erase(const_iterator first, const_iterator last) { return this->Container_.erase(first, last); }

   /// 依照排序加入一個元素.
   /// vtype 可為 const value_type& 或 value_type&&
   template <class vtype>
   std::pair<iterator, bool> insert(vtype&& v) {
      iterator i;
      if (find_i(i, this->begin(), this->end(), v, this->Cmp_))
         return std::make_pair(i, false);
      return std::make_pair(this->Container_.insert(i, std::forward<vtype>(v)), true);
   }
   /// 依照排序加入一個元素.
   /// vtype 可為 const value_type& 或 value_type&&
   template <class vtype>
   iterator insert(iterator ihint, vtype&& v) {
      iterator iend = this->end();
      iterator ibeg = this->begin();
      if (fon9_UNLIKELY(ihint == iend)) {
         if (fon9_UNLIKELY(iend == ibeg)
             || fon9_LIKELY(this->Cmp_(*(ihint - 1), v))) {
            this->Container_.emplace_back(std::forward<vtype>(v));
            return this->Container_.end() - 1;
         }
         ibeg = this->begin();
      }
      else if (fon9_LIKELY(this->Cmp_(v, *ihint))) {
         if (fon9_UNLIKELY(ihint == ibeg)
             || fon9_LIKELY(this->Cmp_(*(ihint - 1), v)))
            return this->Container_.insert(ihint, std::forward<vtype>(v));
         iend = ihint;
      }
      else if (ibeg != ihint) {
         --ibeg;
      }
      iterator ifind;
      if (find_i(ifind, ibeg, iend, v, this->Cmp_))
         return ifind;
      return this->Container_.insert(ifind, std::forward<vtype>(v));
   }

   template <class KeyT>
   iterator       find(const KeyT& key) { return find_i(this->begin(), this->end(), key, this->Cmp_); }
   template <class KeyT>
   const_iterator find(const KeyT& key) const { return find_i(this->begin(), this->end(), key, this->Cmp_); }

   template <class KeyT>
   iterator       lower_bound(const KeyT& key) { return lower_i(this->begin(), this->end(), key, this->Cmp_); }
   template <class KeyT>
   const_iterator lower_bound(const KeyT& key) const { return lower_i(this->begin(), this->end(), key, this->Cmp_); }

   template <class RMap>
   bool is_equal(const RMap& rhs) const {
      return this->Container_ == rhs.Container_;
   }
};
fon9_WARN_POP;

} // namespaces
#endif//__fon9_SortedVector_hpp__
