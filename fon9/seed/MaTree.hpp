/// \file fon9/seed/MaTree.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_MaTree_hpp__
#define __fon9_seed_MaTree_hpp__
#include "fon9/seed/TreeLockContainerT.hpp"
#include "fon9/Named.hpp"
#include "fon9/SortedVector.hpp"

namespace fon9 { namespace seed {

class fon9_API MaTree;
using MaTreeSP = TreeSPT<MaTree>;

/// \ingroup seed
/// 在 fon9 裡面, 通常會將一組相關物件集中, 然後用 **物件名字** 來管理, 例:
/// - 系統最底層用一個 MaTree Root_; 來管理整個應用系統.
/// - MaTree::Add() 提供註冊介面, 提供功能擴充, 例:
///   `Root_.Add(NamedSeedSP{new AuthMgr("AuthMgr")});`
/// - 透過名字從 MaTree 取得服務提供者(NamedSeed), 例:
///   AuthMgrSP authmgr{Root_.Get<AuthMgr>("AuthMgr")};
class fon9_API NamedSeed : public intrusive_ref_counter<NamedSeed>, public Named {
   fon9_NON_COPY_NON_MOVE(NamedSeed);
   using base = Named;

protected:
   /// 在最後一個 NamedSeedSP 死亡時會呼叫此處, 所以此時已沒有任何人擁有 this!
   /// 預設: if (this->use_count() == 0) delete this;
   virtual void OnNamedSeedReleased();
   inline friend void intrusive_ptr_deleter(const NamedSeed* nseed) {
      const_cast<NamedSeed*>(nseed)->OnNamedSeedReleased();
   }

public:
   using Named::Named;
   virtual ~NamedSeed();

   static Fields MakeDefaultFields();

   virtual TreeSP GetSapling();
   virtual void OnSeedCommand(SeedOpResult& res, StrView cmdln, FnCommandResultHandler resHandler);

   /// 應在 Parent Tree 收到 OnParentSeedClear() 時呼叫.
   /// 預設: sapling->OnParentSeedClear();
   virtual void OnParentTreeClear(Tree&);
};

/// \ingroup seed
/// 在建構時就確定是否有 Sapling 的 NamedSeed.
class fon9_API NamedSapling : public NamedSeed {
   fon9_NON_COPY_NON_MOVE(NamedSapling);
   using base = NamedSeed;
public:
   const TreeSP Sapling_;

   template <class... NamedArgsT>
   NamedSapling(TreeSP sapling, NamedArgsT&&... namedargs)
      : base(std::forward<NamedArgsT>(namedargs)...)
      , Sapling_{std::move(sapling)} {
   }

   virtual TreeSP GetSapling() override;
};

template <class NamedSeedBaseT>
using NamedSeedSPT = intrusive_ptr<NamedSeedBaseT>;
using NamedSeedSP = NamedSeedSPT<NamedSeed>;

struct CmpNamedSeedSP {
   bool operator()(const NamedSeedSP& lhs, const NamedSeedSP& rhs) const {
      return lhs->Name_ < rhs->Name_;
   }
   bool operator()(const NamedSeedSP& lhs, const StrView& rhs) const {
      return ToStrView(lhs->Name_) < rhs;
   }
   bool operator()(const StrView& lhs, const NamedSeedSP& rhs) const {
      return lhs < ToStrView(rhs->Name_);
   }
};
using NamedSeedContainerImpl = SortedVectorSet<NamedSeedSP, CmpNamedSeedSP>;

/// \ingroup seed
/// Container of NamedSeedSP.
class fon9_API MaTree : public TreeLockContainerT<NamedSeedContainerImpl> {
   fon9_NON_COPY_NON_MOVE(MaTree);
   using base = TreeLockContainerT<NamedSeedContainerImpl>;
   struct TreeOp;
   struct PodOp;
protected:
   /// 在 Add() 成功返回前(尚未解鎖) 的事件通知.
   virtual void OnMaTree_AfterAdd(Locker&, NamedSeed& seed);
   /// 在 Remove() 成功返回前(尚未解鎖) 的事件通知.
   virtual void OnMaTree_AfterRemove(Locker&, NamedSeed& seed);
   /// 在 OnParentSeedClear() 成功返回前(已解鎖) 的事件通知.
   virtual void OnMaTree_AfterClear();

public:
   /// 預設建構, 預設的Layout = Layout1: 包含一個 Tab: 內含3個欄位: Name, Title, Description;
   MaTree(std::string tabName);
   MaTree(LayoutSP layout) : base{std::move(layout)} {
   }

   NamedSeedSP Remove(StrView name);

   /// \retval false  seed->Name_ 已存在, 如果 !logErrHeader.empty() 則在帆回前會先記錄 log.
   bool Add(NamedSeedSP seed, StrView logErrHeader = nullptr);

   /// Plant a NamedSeed.
   /// - T 必須提供建構: T(MaTreeSP ma, std::string name, args...);
   /// - T 一定會建立一次: 先建立 T, 然後再 Add(), 如果 Add() 失敗, 則會刪除 T.
   /// - 使用範例:
   /// \code
   ///   if (auto dllmgr = ma->Plant<DllMgr>("DllMgr.Plant", "DllMgr")) {
   ///      dllmgr->LoadConfigFile(cfgFileName);
   ///   }
   /// \endcode
   template <class T, class... ArgsT>
   NamedSeedSPT<T> Plant(const StrView& logErrHeader, std::string name, ArgsT&&... args) {
      NamedSeedSPT<T> seed{new T(this, std::move(name), std::forward<ArgsT>(args)...)};
      if (this->Add(seed, logErrHeader))
         return seed;
      return NamedSeedSPT<T>(nullptr);
   }

   /// 請注意 thread safe: 除了 retval->Name_ 是 thread safe,
   /// 其他的 retval->GetTitle(); retval->GetDescription(); 都不是 thread safe!
   NamedSeedSP Get(StrView name) const {
      ConstLocker container{this->Container_};
      auto        ifind{container->find(name)};
      return ifind == container->end() ? NamedSeedSP{} : *ifind;
   }
   template <class T>
   intrusive_ptr<T> Get(StrView name) const {
      return dynamic_pointer_cast<T>(this->Get(name));
   }

   std::vector<NamedSeedSP> GetList(StrView nameHead) const;

   /// 清除全部的元素(Seeds).
   /// - 避免循環相依造成的 resource leak
   /// - 清理時透過 NamedSeed::OnParentTreeClear() 通知 seed.
   virtual void OnParentSeedClear() override;

   virtual void OnTreeOp(FnTreeOp fnCallback) override;
};

} } // namespaces
#endif//__fon9_seed_MaTree_hpp__
