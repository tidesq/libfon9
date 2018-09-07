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

fon9_WARN_DISABLE_PADDING;
/// \ingroup seed
/// 歡迎來到 fon9 forest(seed 機制), 從外界來的 visitor 透過這裡來拜訪 forest.
class fon9_API SeedVisitor : public intrusive_ref_counter<SeedVisitor> {
   fon9_NON_COPY_NON_MOVE(SeedVisitor);
protected:
   friend class TicketRunner;
   const SeedFairySP Fairy_;

public:
   SeedVisitor(MaTreeSP root) : Fairy_{new SeedFairy{std::move(root)}} {
   }
   virtual ~SeedVisitor();

   virtual void SetCurrPath(StrView currPath);

   /// 當 runner 事情完成(成功或失敗), 如果沒有觸發 OnTicketRunnerXXX() 事件, 則透過這裡通知 visitor.
   virtual void OnTicketRunnerDone(TicketRunner& runner, DcQueue&& extmsg) = 0;

   virtual void OnTicketRunnerWrite(TicketRunnerWrite&, const SeedOpResult& res, const RawWr& wr) = 0;
   virtual void OnTicketRunnerRead(TicketRunnerRead&, const SeedOpResult& res, const RawRd& rd) = 0;
   virtual void OnTicketRunnerRemoved(TicketRunnerRemove&, const PodRemoveResult& res) = 0;
   /// 只有在 res.OpResult_ == OpResult::no_error 時, 才會觸發此事件.
   virtual void OnTicketRunnerGridView(TicketRunnerGridView&, GridViewResult& res) = 0;
   virtual void OnTicketRunnerCommand(TicketRunnerCommand&, const SeedOpResult& res, StrView msg) = 0;
   virtual void OnTicketRunnerSetCurrPath(TicketRunnerCommand&) = 0;
};
using SeedVisitorSP = intrusive_ptr<SeedVisitor>;
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

   /// 若有錯誤, 則傳回錯誤訊息, 例如: "fieldName=xxx|err=field not found\n";
   /// 若有多個錯誤, 則會產生多行訊息.
   RevBufferList ParseSetValues(const SeedOpResult& res, const RawWr& wr);
};

/// \ingroup seed
/// 移除一個 seed or pod;
class fon9_API TicketRunnerRemove : public TicketRunner {
   fon9_NON_COPY_NON_MOVE(TicketRunnerRemove);
   using base = TicketRunner;
   FnPodRemoved RemovedHandler_;
   virtual void OnRemoved(const PodRemoveResult& res);
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
   uint16_t    RowCount_;

   TicketRunnerGridView(SeedVisitor& visitor, StrView seed, uint16_t rowCount, StrView startKey, StrView tabName);
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
   TicketRunnerCommand(SeedVisitor& visitor, StrView seed, StrView cmdln)
      : base(visitor, seed, cmdln.empty() ? AccessRight::None : AccessRight::Exec)
      , SeedCommandLine_{cmdln.ToString()} {
   }
   void OnFoundTree(TreeOp&) override;
   void OnLastSeedOp(const PodOpResult& resPod, PodOp* pod, Tab& tab) override;
};

} } // namespaces
#endif//__fon9_seed_TicketRunner_hpp__
