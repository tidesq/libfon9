/// \file fon9/seed/TreeOp.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_TreeOp_hpp__
#define __fon9_seed_TreeOp_hpp__
#include "fon9/seed/RawRd.hpp"
#include "fon9/buffer/RevBufferList.hpp"

namespace fon9 { namespace seed {
fon9_WARN_DISABLE_PADDING;
fon9_MSC_WARN_DISABLE_NO_PUSH(4265 /* class has virtual functions, but destructor is not virtual. */);

//--------------------------------------------------------------------------//

struct PodOpResult {
   /// 此值必定有效, 不會是 nullptr.
   Tree*    Sender_;
   OpResult OpResult_;
   StrView  KeyText_;

   PodOpResult(Tree& sender, OpResult res, const StrView& key)
      : Sender_{&sender}, OpResult_{res}, KeyText_{key} {
   }
   PodOpResult(OpResult res, const StrView& key)
      : Sender_{nullptr}, OpResult_{res}, KeyText_{key} {
   }
};

class fon9_API PodOp;
using FnPodOp = std::function<void(const PodOpResult& res, PodOp* op)>;

struct PodRemoveResult : public PodOpResult {
   PodRemoveResult(Tree& sender, OpResult res, const StrView& key, Tab* tab)
      : PodOpResult{sender, res, key}
      , Tab_{tab} {
   }
   PodRemoveResult(OpResult res, const StrView& key)
      : PodOpResult{res, key}
      , Tab_{nullptr} {
   }
   /// 若為 nullptr 表示 Remove Pod 的結果.
   /// 若為 !nullptr 表示 Remove Seed 的結果.
   Tab*     Tab_;
};
using FnPodRemoved = std::function<void(const PodRemoveResult& res)>;

//--------------------------------------------------------------------------//

struct GridViewRequest {
   /// 在 TreeOp::GetGridView() 實作時要注意:
   /// - 如果要切到另一 thread 處理:
   ///   若 OrigKey_ 不是 TreeOp::kStrKeyText_Begin_; 或 TreeOp::kStrKeyText_End_;
   ///   則 OrigKey_ **必須** 要用 std::string 複製一份.
   StrView  OrigKey_;

   /// nullptr: 只取出 Key.
   /// !nullptr: 指定 Tab.
   Tab*     Tab_{nullptr};

   /// 用 ifind = lower_bound(OrigKey_) 之後.
   /// ifind 移動幾步之後才開始抓取資料;
   int16_t  Offset_{0};

   /// 最多取出幾筆資料, 0 = 不限制(由實作者自行決定).
   uint16_t MaxRowCount_{0};

   /// GridViewResult::GridView_ 最多最大容量限制, 0則表示不限制.
   /// 實際取出的資料量可能 >= MaxBufferSize_;
   uint32_t MaxBufferSize_{0};

   GridViewRequest(const StrView& origKey) : OrigKey_{origKey} {
   }
};

struct GridViewResult {
   fon9_NON_COPY_NON_MOVE(GridViewResult);

   Tree*       Sender_;
   OpResult    OpResult_;

      /// 使用 "\n"(kRowSplitter) 分隔行, 使用 "\x01"(kCellSplitter) 分隔 cell.
   std::string GridView_;

   /// GridView_ 裡面有幾個行(Row)資料?
   uint16_t    RowCount_{0};

   /// GridView_ 裡面第一行距離 begin 多遠?
   /// 如果不支援計算距離, 則為 GridViewResult::kNotSupported;
   size_t      DistanceBegin_{kNotSupported};
   /// GridView_ 裡面最後行距離 end 多遠?
   /// 如果不支援計算距離, 則為 GridViewResult::kNotSupported;
   size_t      DistanceEnd_{kNotSupported};
   size_t      ContainerSize_{kNotSupported};

   GridViewResult(Tree& sender, OpResult res)
      : Sender_(&sender)
      , OpResult_{res} {
   }
   GridViewResult(Tree& sender) : GridViewResult(sender, OpResult::no_error) {
   }

   GridViewResult(OpResult res) : Sender_{nullptr}, OpResult_{res} {
   }

   enum : size_t {
      /// 如果 DistanceBegin_, DistanceEnd_ 不支援, 則等於此值.
      kNotSupported = static_cast<size_t>(-1)
   };
   enum : char {
      kCellSplitter = '\t',
      kRowSplitter = '\n'
   };

   template <class Container>
   auto SetContainerSize(Container& container) -> decltype(container.size(), void()) {
      this->ContainerSize_ = container.size();
   }
   template <class Container>
   void SetContainerSize(...) {
      this->ContainerSize_ = kNotSupported;
   }
};
using FnGridViewOp = std::function<void(GridViewResult& result)>;

//--------------------------------------------------------------------------//

/// \ingroup seed
/// Tree 的(管理)操作不是放在 class Tree's methods? 因為:
/// - 無法在操作前知道如何安全的操作 tree:
///   - mutex lock? AQueue? or 切到指定的 thread? 還是用其他方法?
///   - 只有 tree 自己知道何時何地才能安全的操作.
///   - 所以使用 callback(FnTreeOp) 的方式, 提供操作 tree 的物件: TreeOp.
/// - 必須透過 Tree::OnTreeOp(FnOnTreeOp fnCallback);
///   - 然後在 fnCallback(OpResult res, TreeOp* op) 裡面,
///   - 對 op 進行操作(thread safe).
class fon9_API TreeOp {
   fon9_NON_COPY_NON_MOVE(TreeOp);
protected:
   ~TreeOp() {
   }

   TreeOp(Tree& tree) : Tree_(tree) {
   }

public:
   Tree& Tree_;

   /// 當用此當作 OnPodOp(), OnPodOpRange() 的參數時, 則表示 container->begin();
   static constexpr const char* const  kStrKeyText_Begin_ = nullptr;
   static constexpr StrView TextBegin() {
      return StrView{kStrKeyText_Begin_, static_cast<size_t>(0)};
   }
   static constexpr bool IsTextBegin(const StrView& k) {
      return k.begin() == kStrKeyText_Begin_;
   }

   /// 當用此當作 OnPodOp(), OnPodOpRange() 的參數時, 則表示 container->end();
   /// 也可用在支援 append 的 Tree, 例: AddPod(StrView{TreeOp::kStrKeyText_End_,0}); 用來表示 append.
   static constexpr const char* const  kStrKeyText_End_ = reinterpret_cast<const char*>(-1);
   static constexpr StrView TextEnd() {
      return StrView{kStrKeyText_End_, static_cast<size_t>(0)};
   }
   static constexpr bool IsTextEnd(const StrView& k) {
      // return k.begin() == kStrKeyText_End_; 這樣寫, VC 不允許: C3249: illegal statement or sub-expression for 'constexpr' function
      return reinterpret_cast<intptr_t>(k.begin()) == reinterpret_cast<intptr_t>(kStrKeyText_End_);
   }

   template <class Container, class Iterator>
   static bool GetStartIterator(Container& container, Iterator& istart, const char* strKeyText) {
      if (strKeyText == kStrKeyText_Begin_)
         istart = container.begin();
      else if (strKeyText == kStrKeyText_End_)
         istart = container.end();
      else
         return false;
      return true;
   }
   template <class Container, class FnStrToKey, class Iterator = typename Container::iterator, class KeyT = typename Container::key_type>
   static Iterator GetStartIterator(Container& container, StrView strKeyText, FnStrToKey fnStrToKey) {
      Iterator ivalue;
      if (GetStartIterator(container, ivalue, strKeyText.begin()))
         return ivalue;
      return container.lower_bound(fnStrToKey(strKeyText));
   }

   virtual void GridView(const GridViewRequest& req, FnGridViewOp fnCallback);

   template <class Container, class FnStrToKey, class Iterator = typename Container::iterator, class KeyT = typename Container::key_type>
   static Iterator GetFindIterator(Container& container, StrView strKeyText, FnStrToKey fnStrToKey) {
      Iterator ivalue;
      if (GetStartIterator(container, ivalue, strKeyText.begin()))
         return ivalue;
      return container.find(fnStrToKey(strKeyText));
   }
   /// 增加一個 pod.
   /// - 當 key 存在時, 視為成功, 會呼叫: fnCallback(OpResult::key_exists, op);
   /// - 有些 tree 不允許從管理介面加入 pod, 此時會呼叫: fnCallback(OpResult::not_supported_add_pod, nullptr);
   /// - 若 tree 有支援 append(), 則 strKeyText 可使用 TreeOp::TextEnd();
   virtual void Add(StrView strKeyText, FnPodOp fnCallback);
   virtual void Get(StrView strKeyText, FnPodOp fnCallback);

   /// - tab == nullptr: 移除 pod.
   /// - tab != nullptr
   ///   - 可能僅移除某 seed.
   ///   - 或將該 seed 資料清除(變成初始狀態).
   ///   - 或不支援.
   ///   - 由實作者決定.
   virtual void Remove(StrView strKeyText, Tab* tab, FnPodRemoved fnCallback);
};
fon9_WARN_POP;

//--------------------------------------------------------------------------//

/// \ingroup seed
/// - 若 tab != nullptr 則 not_supported_remove_seed
/// - 若 tab == nullptr 則 container.find() & container.erase();
template <class Container, class Key = typename Container::key_type>
void RemovePod(Tree& sender, Container& container, const Key& key, StrView strKeyText, Tab* tab, FnPodRemoved&& fnCallback) {
   PodRemoveResult res{sender, OpResult::not_supported_remove_seed, strKeyText, tab};
   if (tab == nullptr) {
      auto  i = container.find(key);
      if (i == container.end())
         res.OpResult_ = OpResult::not_found_key;
      else {
         container.erase(i);
         res.OpResult_ = OpResult::removed_pod;
      }
   }
   fnCallback(res);
}

/// \ingroup seed
/// 協助 TreeOp::Get().
/// TreeT 必須提供 TreeT::PodOp;
template <class TreeT, class Container, class Iterator = typename Container::iterator>
void GetPod(TreeT& sender, Container& container, Iterator ivalue, StrView strKeyText, FnPodOp&& fnCallback) {
   if (ivalue == container.end())
      fnCallback(PodOpResult{sender, OpResult::not_found_key, strKeyText}, nullptr);
   else {
      typename TreeT::PodOp op{*ivalue, sender, OpResult::no_error, strKeyText};
      fnCallback(op, &op);
   }
}

/// \ingroup seed
/// 將欄位字串輸出到 rbuf, 每個欄位前都會加上 chSplitter 字元.
fon9_API void FieldsCellRevPrint(const Fields& fields, const RawRd& rd, RevBuffer& rbuf, char chSplitter);

template <class Iterator>
void SimpleMakeRowView(Iterator ivalue, Tab* tab, RevBuffer& rbuf) {
   if (tab)
      FieldsCellRevPrint(tab->Fields_, SimpleRawRd{*ivalue}, rbuf, GridViewResult::kCellSplitter);
   RevPrint(rbuf, ivalue->first);
}

/// \ingroup seed
/// 協助 TreeOp::GridView().
/// fnRowAppender 可參考 SimpleMakeRowView();
/// 最後一列不含 kRowSplitter。
template <class Iterator, class FnRowAppender>
void MakeGridViewRange(Iterator istart, Iterator ibeg, Iterator iend,
                       const GridViewRequest& req, GridViewResult& res,
                       FnRowAppender fnRowAppender) {
   auto offset = req.Offset_;
   if (offset < 0) {
      while (istart != ibeg) {
         --istart;
         if (++offset >= 0)
            break;
      }
   }
   else if (offset > 0) {
      while (istart != iend) {
         ++istart;
         if (--offset <= 0)
            break;
      }
   }
   res.DistanceBegin_ = (istart == ibeg) ? 0u : static_cast<size_t>(GridViewResult::kNotSupported);
   if (istart == iend)
      res.DistanceEnd_ = 0;
   else {
      RevBufferList  rbuf{256};
      for (;;) {
         fnRowAppender(istart, req.Tab_, rbuf);
         if (!res.GridView_.empty())
            res.GridView_.push_back(res.kRowSplitter);
         BufferAppendTo(rbuf.MoveOut(), res.GridView_);
         ++res.RowCount_;
         if (++istart == iend)
            break;
         if (req.MaxRowCount_ > 0 && res.RowCount_ >= req.MaxRowCount_)
            break;
         if (req.MaxBufferSize_ > 0 && (res.GridView_.size() >= req.MaxBufferSize_))
            break;
      }
      res.DistanceEnd_ = (istart == iend) ? 1u : static_cast<size_t>(GridViewResult::kNotSupported);
   }
}

/// \ingroup seed
/// 協助 TreeOp::GridView().
/// fnRowAppender 可參考 SimpleMakeRowView();
template <class Container, class Iterator, class FnRowAppender>
void MakeGridView(Container& container, Iterator istart,
                  const GridViewRequest& req, GridViewResult& res,
                  FnRowAppender&& fnRowAppender) {
   res.SetContainerSize(container);
   MakeGridViewRange(istart, container.begin(), container.end(),
                     req, res, std::forward<FnRowAppender>(fnRowAppender));
}

/// \ingroup seed
/// 協助 TreeOp::GridView().
/// fnRowAppender 可參考 SimpleMakeRowView(); 但須傳回 true 表示有資料, false 表示無資料.
/// 最後一列不含 kRowSplitter。
template <class Iterator, class FnRowAppender>
void MakeGridViewArrayRange(Iterator istart, Iterator iend,
                            const GridViewRequest& req, GridViewResult& res,
                            FnRowAppender fnRowAppender) {
   if (req.Offset_ < 0) {
      if (istart >= static_cast<Iterator>(-req.Offset_))
         istart += req.Offset_;
      else
         istart = 0;
   }
   else if (req.Offset_ > 0) {
      if (iend - istart <= static_cast<Iterator>(req.Offset_))
         istart = iend;
      else
         istart += req.Offset_;
   }
   res.DistanceBegin_ = istart;
   if ((res.DistanceEnd_ = (iend - istart)) != 0) {
      RevBufferList  rbuf{256};
      for (;;) {
         if (!fnRowAppender(istart, req.Tab_, rbuf)) {
            assert(rbuf.cfront() == nullptr);
            if (++istart == iend)
               break;
            continue;
         }
         if (!res.GridView_.empty())
            res.GridView_.push_back(res.kRowSplitter);
         BufferAppendTo(rbuf.MoveOut(), res.GridView_);
         ++res.RowCount_;
         if (++istart == iend)
            break;
         if (req.MaxRowCount_ > 0 && res.RowCount_ >= req.MaxRowCount_)
            break;
         if (req.MaxBufferSize_ > 0 && (res.GridView_.size() >= req.MaxBufferSize_))
            break;
      }
   }
}

//--------------------------------------------------------------------------//

template <class TreeOp, class MustLockContainer, class FnRowAppender>
void TreeOp_GridView_MustLock(TreeOp& op, MustLockContainer& c,
                              const GridViewRequest& req, FnGridViewOp&& fnCallback,
                              FnRowAppender&& fnRowAppender) {
   GridViewResult res{op.Tree_};
   {
      typename MustLockContainer::Locker container{c};
      MakeGridView(*container, op.GetStartIterator(*container, req.OrigKey_),
                   req, res, std::move(fnRowAppender));
   } // unlock.
   fnCallback(res);
}

template <class PodOp, class TreeOp, class MustLockContainer>
void TreeOp_Get_MustLock(TreeOp& treeOp, MustLockContainer& c, StrView strKeyText, FnPodOp&& fnCallback) {
   {
      typename MustLockContainer::Locker container{c};
      auto   ifind = treeOp.GetStartIterator(*container, strKeyText);
      if (ifind != container->end()) {
         PodOp podOp{*ifind, treeOp.Tree_, OpResult::no_error, strKeyText, container};
         fnCallback(podOp, &podOp);
         return;
      }
   } // unlock.
   fnCallback(PodOpResult{treeOp.Tree_, OpResult::not_found_key, strKeyText}, nullptr);
}

} } // namespaces
#endif//__fon9_seed_TreeOp_hpp__
