/// \file fon9/NamedIx.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_NamedIx_hpp__
#define __fon9_NamedIx_hpp__
#include "fon9/Named.hpp"
#include <map>
#include <vector>
#include <memory>

namespace fon9 {

template <class NamedIxSP>
class NamedIxMapHasRemove;

fon9_WARN_DISABLE_PADDING;
/// \ingroup seed
/// 有索引位置的命名物件.
/// - 在 NamedIxMapNoRemove, NamedMapWithRemove 裡面.
/// - 除了用 name 來找物件.
/// - 也可以直接使用 index 來取得物件.
class fon9_API NamedIx : public Named {
   fon9_NON_COPY_NON_MOVE(NamedIx);
   int   Index_{-1};
   template <class NamedIxSP> friend class NamedIxMapHasRemove;

public:
   using Named::Named;

   NamedIx(const Named& named) : Named(named) {
   }
   NamedIx(Named&& named) : Named(std::move(named)) {
   }

   /// 命名物件的索引位置.
   /// 要加入 NamedMap 之後, 此索引才會正確: >=0
   int GetIndex() const {
      return Index_;
   }
   /// \retval true  設定 index 成功.
   /// \retval false 設定 index 失敗: index只能設定一次.
   bool SetIndex(size_t index) {
      if (this->Index_ >= 0)
         return false;
      this->Index_ = static_cast<int>(index);
      return true;
   }
};
fon9_WARN_POP;

/// \ingroup seed
/// \tparam NamedIxSPT 必須是 NamedIx 衍生類的智慧指標, 例:
///                    shared_ptr<NamedIx>; unique_ptr<NamedIx>;
template <class NamedIxSPT>
class NamedIxMapNoRemove {
protected:
   /// key = 名稱, value = index in Ary.
   typedef std::map<StrView, size_t>   Map;
   /// 依照加入順序存放在陣列尾端.
   typedef std::vector<NamedIxSPT>     Ary;
   Map   Map_;
   Ary   Ary_;

public:
   using NamedIxSP = NamedIxSPT;
   using NamedIxPtr = typename NamedIxSP::pointer;
   using value_type = typename NamedIxSP::element_type;

   /// \retval true  成功將 aobj 加入
   /// \retval false 加入失敗: !aobj 或 aobj->Name_ 重複, 或 (aobj->GetIndex() >= 0)
   bool Add(NamedIxSP aobj) {
      if (!aobj)
         return false;
      typename Map::value_type   aval(&aobj->Name_, this->Ary_.size());
      auto insr = this->Map_.insert(aval);
      if (!insr.second)
         return false;
      if (!aobj->SetIndex(aval.second)) {
         this->Map_.erase(insr.first);
         return false;
      }
      this->Ary_.emplace_back(std::move(aobj));
      return true;
   }

   /// 用索引取得命名物件.
   /// \retval nullptr 無效的索引.
   NamedIxPtr Get(size_t index) const {
      if (index >= this->Ary_.size())
         return nullptr;
      return this->Ary_[index].get();
   }

   /// 用名稱取得命名物件.
   NamedIxPtr Get(StrView name) const {
      typename Map::const_iterator  ifind = this->Map_.find(name);
      if (ifind == this->Map_.end())
         return nullptr;
      return this->Get(ifind->second);
   }

   /// 用名稱取得命名物件索引.
   /// \retval -1 無效的物件名稱.
   int GetIndex(StrView name) const {
      typename Map::const_iterator  ifind = this->Map_.find(name);
      if (ifind == this->Map_.end())
         return -1;
      return static_cast<int>(ifind->second);
   }

   size_t size() const {
      return this->Ary_.size();
   }

   /// 依照 index 順序, 取出全部的 NamedIxSP::pointer.
   /// \retval this->size()
   size_t GetAll (std::vector<NamedIxPtr>& ptrs) const {
      const size_t count = this->size();
      ptrs.resize(count);
      for (size_t L = 0; L < count; ++L)
         ptrs[L] = this->Get(L);
      return count;
   }
};

/// \ingroup seed
/// 支援移除元素, 但您必須透過繼承的方式, 提供元素移除前後的處理函式.
template <class NamedIxSPT>
class NamedIxMapHasRemove : public NamedIxMapNoRemove<NamedIxSPT> {
   typedef NamedIxMapNoRemove<NamedIxSPT> base;

public:
   using NamedIxSP = NamedIxSPT;
   using NamedIxPtr = typename base::NamedIxPtr;

   virtual ~NamedIxMapHasRemove() {
   }

   /// 移除指定名稱的物件.
   /// \retval false 找不到指定的名稱, 沒有移除任何東西.
   /// \retval true  移除了指定名稱的物件.
   bool Remove(StrView name) {
      typename base::Map::iterator  ifind = this->Map_.find(name);
      if (ifind == this->Map_.end())
         return false;
      this->OnRemove(ifind);
      return true;
   }

   /// 移除指定索引位置的物件.
   bool Remove(size_t index) {
      if (index >= this->Ary_.size())
         return false;
      return Remove(&this->Ary_[index]->Name_);
   }

   /// 移除指定的物件.
   bool Remove(NamedIxPtr aobj) {
      if (!aobj)
         return false;
      size_t index = static_cast<size_t>(aobj->GetIndex());
      if (index >= this->Ary_.size())
         return false;
      if (this->Ary_[index].get() == aobj)
         return Remove(&aobj->Name_);
      return false;
   }

protected:
   /// 元素從 container 移除之前的通知, 此時 container 還包含此元素.
   virtual void OnBeforeRemove(NamedIxPtr aobj) = 0;

   /// 元素從 container 移除之後的通知, 此時 container 已不包含此元素.
   /// 通知完畢之後 NamedIxSP 才會釋放 aobj.
   virtual void OnAfterRemove(NamedIxPtr aobj) = 0;

   virtual void OnRemove(typename base::Map::iterator ifind) {
      size_t     aidx = ifind->second;
      NamedIxSP& aobj = this->Ary_[aidx];
      this->OnBeforeRemove(aobj.get());
      NamedIxSP  pobj = std::move(aobj);
      this->Map_.erase(ifind);
      for (auto imap : this->Map_) {
         if (imap->second > aidx)
            --imap->second;
      }
      auto iary = this->Ary_.erase(this->Ary_.begin() + static_cast<int>(aidx));
      for (; iary != this->Ary_.end(); ++iary)
         --(iary->Index_);
      this->OnAfterRemove(pobj.get());
      this->OnAfterRemove(pobj.get());
      pobj->Index_ = -1;
   }
};

} // namespaces
#endif//__fon9_NamedIx_hpp__
