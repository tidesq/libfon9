/// \file fon9/seed/SeedVisitor.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_TicketRunner_hpp__
#define __fon9_seed_TicketRunner_hpp__
#include "fon9/seed/SeedFairy.hpp"
#include "fon9/buffer/DcQueue.hpp"

namespace fon9 { namespace seed {

class fon9_API TicketRunnerError;
class fon9_API TicketRunnerRead;
class fon9_API TicketRunnerWrite;
class fon9_API TicketRunnerRemove;
class fon9_API TicketRunnerGridView;
class fon9_API TicketRunnerCommand;
class fon9_API TicketRunnerSubscribe;

fon9_WARN_DISABLE_PADDING;
class fon9_API VisitorSubr;
using VisitorSubrSP = intrusive_ptr<VisitorSubr>;

/// \ingroup seed
/// 歡迎來到 fon9 forest(seed 機制), 從外界來的 visitor 透過這裡來拜訪 forest.
class fon9_API SeedVisitor : public intrusive_ref_counter<SeedVisitor> {
   fon9_NON_COPY_NON_MOVE(SeedVisitor);

   /// SeedVisitor 提供訂閱「一個」TreeOp::Subscribe();
   /// 因為 SeedVisitor 通常用在管理, 並不需要多個訂閱.
   /// 實際上 TreeOp::Subscribe() 機制, 沒限制訂閱數量.
   using Subr = MustLock<VisitorSubrSP>;
   Subr  Subr_;
   friend class TicketRunnerSubscribe;
   std::string UFrom_;

protected:
   friend class TicketRunner;
   const SeedFairySP Fairy_;
   VisitorSubrSP NewSubscribe();
   VisitorSubrSP GetSubr() {
      return *this->Subr_.Lock();
   }

   /// 如果有正在執行的 TicketRunner, 則變動 UFrom_ 是危險的!!
   /// 您必須確定沒有 正在執行的 TicketRunner 才能變動此值.
   void SetUFrom(std::string ufrom) {
      this->UFrom_ = std::move(ufrom);
   }

public:
   SeedVisitor(MaTreeSP root, std::string ufrom);
   virtual ~SeedVisitor();

   /// User + From(DeviceId);
   /// e.g. "U=fonwin|R=127.0.0.1:12345|L=127.0.0.1:6080
   const std::string& GetUFrom() const {
      return this->UFrom_;
   }

   virtual void SetCurrPath(StrView currPath);

   /// 當 runner 事情完成(成功或失敗), 如果沒有觸發 OnTicketRunnerXXX() 事件, 則透過這裡通知 visitor.
   virtual void OnTicketRunnerDone(TicketRunner& runner, DcQueue&& extmsg) = 0;

   virtual void OnTicketRunnerWrite(TicketRunnerWrite&, const SeedOpResult& res, const RawWr& wr) = 0;
   virtual void OnTicketRunnerRead(TicketRunnerRead&, const SeedOpResult& res, const RawRd& rd) = 0;
   virtual void OnTicketRunnerRemoved(TicketRunnerRemove&, const PodRemoveResult& res) = 0;
   
   /// 只有在 res.OpResult_ == OpResult::no_error 時, 才會觸發此事件.
   virtual void OnTicketRunnerGridView(TicketRunnerGridView&, GridViewResult& res) = 0;
   /// 在執行 opTree.GridView() 之前的通知, 預設 do nothing.
   /// - 您可以在此直接呼叫 opTree.Subscribe() 先訂閱異動.
   /// - 也可以調整 req 的內容.
   virtual void OnTicketRunnerBeforeGridView(TicketRunnerGridView&, TreeOp& opTree, GridViewRequest& req);

   virtual void OnTicketRunnerCommand(TicketRunnerCommand&, const SeedOpResult& res, StrView msg) = 0;
   virtual void OnTicketRunnerSetCurrPath(TicketRunnerCommand&) = 0;

   /// 訂閱後的事件通知.
   virtual void OnSeedNotify(VisitorSubr& subr, const SeedNotifyArgs& args) = 0;
   /// isSubOrUnsub == true  訂閱成功.
   /// isSubOrUnsub == false 取消訂閱成功.
   virtual void OnTicketRunnerSubscribe(TicketRunnerSubscribe&, bool isSubOrUnsub) = 0;

   void Unsubscribe();
};
using SeedVisitorSP = intrusive_ptr<SeedVisitor>;

class fon9_API VisitorSubr : public intrusive_ref_counter<VisitorSubr> {
   fon9_NON_COPY_NON_MOVE(VisitorSubr);
   CharVector  Path_;
   TreeSP      Tree_;
   Tab*        Tab_{};
   SubConn     SubConn_{};
public:
   const SeedVisitorSP  Visitor_;
   VisitorSubr(SeedVisitor& visitor)
      : Visitor_{&visitor} {
   }
   void Unsubscribe();
   void OnSeedNotify(const SeedNotifyArgs& args);
   const CharVector& GetPath() const {
      return this->Path_;
   }
   Tab* GetTab() const {
      return this->Tab_;
   }
   Tree* GetTree() const {
      return this->Tree_.get();
   }

   /// 執行 opTree.Subscribe(): 只能呼叫一次.
   OpResult Subscribe(StrView path, Tab& tab, TreeOp& opTree);
};
fon9_WARN_POP;

//--------------------------------------------------------------------------//

class fon9_API TicketRunnerError : public TicketRunner {
   fon9_NON_COPY_NON_MOVE(TicketRunnerError);
   using base = TicketRunner;
public:
   TicketRunnerError(SeedVisitor& visitor, OpResult errn, const StrView& errmsg)
      : base(visitor, errn, errmsg) {
   }
   void OnFoundTree(TreeOp&) override;
   void OnLastSeedOp(const PodOpResult&, PodOp*, Tab&) override;

   static TicketRunnerSP ArgumentsMustBeEmpty(SeedVisitor& visitor);
   static TicketRunnerSP UnknownCommand(SeedVisitor& visitor);
};

/// \ingroup seed
/// 如果最後期望找到的是 tree (e.g. PrintLayout, GridView...), 則透過此 runner 來跑.
/// 只要覆寫: `void OnFoundTree(TreeOp& op) override;` 即可操作期望的 tree.
class fon9_API TicketRunnerTree : public TicketRunner {
   fon9_NON_COPY_NON_MOVE(TicketRunnerTree);
   using base = TicketRunner;
public:
   TicketRunnerTree(SeedVisitor& visitor, StrView seed, AccessRight needsRights = AccessRight::Read)
      : base(visitor, seed, needsRights) {
   }
   void OnLastStep(TreeOp& opTree, StrView keyText, Tab& tab) override;
   void OnLastSeedOp(const PodOpResult&, PodOp*, Tab&) override;
};

/// \ingroup seed
/// 期望讀取 seed 的欄位.
class fon9_API TicketRunnerRead : public TicketRunner {
   fon9_NON_COPY_NON_MOVE(TicketRunnerRead);
   using base = TicketRunner;
   void OnReadOp(const SeedOpResult& res, const RawRd* rd);
public:
   TicketRunnerRead(SeedVisitor& visitor, StrView seed)
      : base(visitor, seed, AccessRight::Read) {
   }
   void OnFoundTree(TreeOp&) override;
   void OnLastSeedOp(const PodOpResult& resPod, PodOp* pod, Tab& tab) override;
};

/// \ingroup seed
/// 期望寫入 seed 的欄位.
class fon9_API TicketRunnerWrite : public TicketRunner {
   fon9_NON_COPY_NON_MOVE(TicketRunnerWrite);
   using base = TicketRunner;
   void OnWriteOp(const SeedOpResult& res, const RawWr* wr);
public:
   const std::string FieldValues_;
   TicketRunnerWrite(SeedVisitor& visitor, StrView seed, StrView fldValues)
      : base(visitor, seed, AccessRight::Write)
      , FieldValues_{fldValues.ToString()} {
   }
   void OnFoundTree(TreeOp&) override;
   void OnLastStep(TreeOp& op, StrView keyText, Tab& tab) override;
   void OnLastSeedOp(const PodOpResult& resPod, PodOp* pod, Tab& tab) override;

   /// - 使用 ',' 分隔 fieldName1=value1,fieldName2=value2
   /// - 必要時 value 可以用引號框起來.
   /// - 若有錯誤, 則傳回錯誤訊息, 例如: "fieldName=xxx|err=field not found\n";
   ///   若有多個錯誤, 則會產生多行訊息, 最後行也有 '\n' 結尾.
   RevBufferList ParseSetValues(const SeedOpResult& res, const RawWr& wr);
};

/// \ingroup seed
/// 移除一個 seed or pod;
class fon9_API TicketRunnerRemove : public TicketRunner {
   fon9_NON_COPY_NON_MOVE(TicketRunnerRemove);
   using base = TicketRunner;
   FnPodRemoved RemovedHandler_;
   virtual void OnRemoved(const PodRemoveResult& res);
   void OnBeforeRemove(TreeOp& opTree, StrView keyText, Tab* tab) override;
   void OnAfterRemove(const PodRemoveResult& res) override;
public:
   TicketRunnerRemove(SeedVisitor& visitor, StrView seed);
   void OnFoundTree(TreeOp&) override;
   void OnLastSeedOp(const PodOpResult&, PodOp*, Tab&) override;
   void ContinuePod(TreeOp& opTree, StrView keyText, StrView tabName) override;
};

fon9_WARN_DISABLE_PADDING;
/// \ingroup seed
/// 取得 gird view.
class fon9_API TicketRunnerGridView : public TicketRunnerTree {
   fon9_NON_COPY_NON_MOVE(TicketRunnerGridView);
   using base = TicketRunnerTree;
   CharVector  StartKeyBuf_;
   StrView     StartKey_;
   CharVector  TabName_;
   CharVector  LastKey_; // for Continue();
   void OnGridViewOp(GridViewResult& res);
public:
   using ReqMaxRowCountT = int16_t;
   /// - 若 < 0 則表示:
   ///   從 StartKey_ 往前 n-1(因為要包含 StartKey_) 步之後, 取得 n 筆.
   ///   所以如果 ReqMaxRowCount_ == -1, 其實與 ReqMaxRowCount_ == 1 結果相同.
   ///   如果超過 container.begin(), 則取出的資料可能會超過 StartKey_;
   ReqMaxRowCountT   ReqMaxRowCount_;

   TicketRunnerGridView(SeedVisitor& visitor, StrView seed, ReqMaxRowCountT reqMaxRowCount, StrView startKey, StrView tabName);
   void OnFoundTree(TreeOp& opTree) override;
   /// 接續上次最後的 key 繼續查詢.
   void Continue();
};
using TicketRunnerGridViewSP = intrusive_ptr<TicketRunnerGridView>;
fon9_WARN_POP;

/// \ingroup seed
/// 期望執行 PodOp::OnSeedCommand().
class fon9_API TicketRunnerCommand : public TicketRunner {
   fon9_NON_COPY_NON_MOVE(TicketRunnerCommand);
   using base = TicketRunner;
   const std::string SeedCommandLine_;
   void SetNewCurrPath();
   void OnSeedCommandResult(const SeedOpResult& res, StrView msg);
public:
   /// 如果 cmdln.empty() 則表示切換現在路徑: this->Visitor_->SetCurrPath(ToStrView(this->Path_));
   TicketRunnerCommand(SeedVisitor& visitor, StrView seed, StrView cmdln);
   void OnFoundTree(TreeOp&) override;
   void OnLastSeedOp(const PodOpResult& resPod, PodOp* pod, Tab& tab) override;
};

class fon9_API TicketRunnerSubscribe : public TicketRunnerTree {
   fon9_NON_COPY_NON_MOVE(TicketRunnerSubscribe);
   using base = TicketRunnerTree;
   TicketRunnerSubscribe(SeedVisitor& visitor, StrView seed, VisitorSubr* subr);
public:
   const CharVector     TabName_;
   const VisitorSubrSP  Subr_;
   /// 新增註冊.
   TicketRunnerSubscribe(SeedVisitor& visitor, StrView seed, StrView tabName);
   /// 取消註冊.
   TicketRunnerSubscribe(SeedVisitor& visitor, StrView seed)
      : TicketRunnerSubscribe{visitor, seed, visitor.Subr_.Lock()->get()} {
   }

   void OnFoundTree(TreeOp& opTree) override;
};

} } // namespaces
#endif//__fon9_seed_TicketRunner_hpp__
