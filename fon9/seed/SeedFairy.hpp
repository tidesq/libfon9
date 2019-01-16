/// \file fon9/seed/SeedFairy.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_SeedFairy_hpp__
#define __fon9_seed_SeedFairy_hpp__
#include "fon9/seed/MaTree.hpp"
#include "fon9/seed/SeedAcl.hpp"
#include "fon9/seed/SeedSearcher.hpp"

namespace fon9 { namespace seed {

class fon9_API SeedVisitor;
using SeedVisitorSP = intrusive_ptr<SeedVisitor>;

class fon9_API TicketRunner;
using TicketRunnerSP = intrusive_ptr<TicketRunner>;

fon9_WARN_DISABLE_PADDING;
/// \ingroup seed
/// 一個 fairy 負責引道一個 visitor 在森林(Tree、Pod、Seed...)間穿梭.
/// - 所有的操作都 **不是 thread safe**
/// - 檢查訪問的權限: NormalizeSeedPath()
class fon9_API SeedFairy : public intrusive_ref_counter<SeedFairy> {
   fon9_NON_COPY_NON_MOVE(SeedFairy);
   struct AclTree;
   AclConfig   Ac_;
   AclPath     CurrPath_;
public:
   /// 透過 "/../" 來存取的資料表, e.g. "/../Acl" 可以查看 this->Ac_.Acl_ 的內容.
   const MaTreeSP VisitorsTree_;
   const MaTreeSP Root_;

   SeedFairy(MaTreeSP root);
   ~SeedFairy();

   void Clear() {
      this->Ac_.Clear();
      this->CurrPath_.clear();
   }

   const AclPath& GetCurrPath() const {
      return this->CurrPath_;
   }
   void SetCurrPath(StrView currPath) {
      this->CurrPath_.assign(currPath);
   }
   void SetCurrPathToHome() {
      this->SetCurrPath(ToStrView(this->Ac_.Home_));
   }

   /// 重設 AclConfig 然後 SetCurrPathToHome();
   void ResetAclConfig(AclConfig&& cfg) {
      this->Ac_ = std::move(cfg);
      this->SetCurrPathToHome();
   }
   void ResetAclConfig(const AclConfig& cfg) {
      this->Ac_ = cfg;
      this->SetCurrPathToHome();
   }

   /// - 正規化路徑, 並檢查訪問權限.
   /// - 正規化的前置路徑:
   ///   - if(seed.Get1st()=='~') 則將 this->Ac_.Home_ 加到 seed 前端後再正規畫.
   ///   - if(seed.Get1st()=='/') 則直接正規畫 seed.
   ///   - 其餘, 則將 this->CurrPath_ 加到 seed 前端後再正規畫.
   /// - 不論結果如何, 返回時:
   ///   - seed 指向 outpath
   ///   - *reRights = 實際的可用權限.
   ///
   /// \retval OpResult::no_error 正規化成功, 且有期望的權限.
   /// \retval OpResult::path_format_error 格式有錯, seed.begin() 為 outpath 的錯誤位置.
   /// \retval OpResult::access_denied 無權限:
   ///      - 若 *reRights != AccessRight::None, 則必須要有 *reRights 的完整權限.
   ///      - 若 *reRights == AccessRight::None, 則有任一權限即可.
   OpResult NormalizeSeedPath(StrView& seed, AclPath& outpath, AccessRight* reRights) const;

   /// - 先透過 NormalizeSeedPath()「正規化 path & 檢查權限」, 如果有權限, 則會傳回:
   ///   - if (!IsVisitorsTree(path)) 傳回 this->Root_;
   ///   - if (IsVisitorsTree(path))  傳回 this->VisitorsTree_;
   /// - 如果傳回 nullptr, 則應檢查 opResult, 請參閱 NormalizeSeedPath() 的傳回值.
   /// - 不論是否成功, 返回時:
   ///   - seed 指向 outpath
   ///   - *reRights = 實際的可用權限.
   MaTree* GetRootPath(OpResult& opResult, StrView& seed, AclPath& outpath, AccessRight* reRights) const;

   /// 根據 cmdln 建立對應的 TicketRunner.
   /// - 取得 runner 之後, 您必須自行決定何時執行 runner.Run();
   /// - cmdln 格式:
   ///   - 開頭為引號「' " `」或「/ . ~」表示為要切換 CurrPath 或執行 SeedCommand.
   ///     此時建立的是 TicketRunnerCommand;
   ///   - 否則為特定指令, 格視為 `指令,指令參數 [seedName]`, 若 "seedName" 不存在, 則使用 GetCurrPath();
   ///   - ss,fld1=val1,fld2=val2... [seedName]
   ///     - Set seed field value. 若 seed 不存在則會建立, 建立後再設定 field 的值.
   ///     - 返回 TicketRunnerWrite
   ///     - 透過 SeedVisitor::OnTicketRunnerWrite(); 解析 FieldValues_ 並填入 `RawWr& wr`.
   ///   - ps [seedName]
   ///     - Print(read) seed values.
   ///     - 返回 TicketRunnerRead
   ///     - 透過 SeedVisitor::OnTicketRunnerRead(); 取得 `RawRd& rd`.
   ///   - rs [seedName]
   ///     - Remove seed(or pod).
   ///     - 返回 TicketRunnerRemove
   ///     - 若成功, 則透過 SeedVisitor::OnTicketRunnerRemoved(); 通知.
   ///   - gv[,[rowCount][,startKey[^tabName]]] [seedName]
   ///     - Grid view.
   ///     - 返回 TicketRunnerGridView
   ///     - 若成功, 則透過 SeedVisitor::OnTicketRunnerGridView(); 通知.
   ///     - 範例:
   ///       - gv,rowCount    [seedName]   指定 rowCount, rowCount>=0:從 begin 開始; rowCount<0: 從 end 開始; 使用 Tab[0]
   ///       - gv,,startKey   [seedName]   rowCount=0(使用預設), 從指定的 startKey 開始, 使用 Tab[0]
   ///       - gv,,^tabName   [seedName]   指定 tabName, 從 begin 開始
   ///       - gv,,''^tabName [seedName]   指定 tabName, 從 key='' 開始
   ///   - 如果為不認識的指令, 則返回 nullptr.
   struct fon9_API Request {
      TicketRunnerSP Runner_;
      StrView        Command_;
      StrView        CommandArgs_;
      StrView        SeedName_;
      Request(SeedVisitor& visitor, StrView cmdln);
   };
};
using SeedFairySP = intrusive_ptr<SeedFairy>;

/// \ingroup seed
/// 到達指定地點的票.
/// 這裡指示出到達的地點, 但不保證可以到達.
struct fon9_API SeedTicket {
   fon9_NON_COPY_NON_MOVE(SeedTicket);
   OpResult          OpResult_;
   /// 實際在 Acl 設定的可用權限.
   const AccessRight Rights_{AccessRight::None};
   const AclPath     Path_;
   const MaTreeSP    Root_{nullptr};
   /// 在到達指定地點前, 有可能在某處中斷, 中斷原因可從 OpResult_ 得到,
   /// 而這裡則記錄了發生錯誤的位置 = Path_.begin() + ErrPos_;
   size_t            ErrPos_{0};

   /// 透過 fairy.GetRootPath() 建構.
   SeedTicket(SeedFairy& fairy, StrView& seed, AccessRight needsRights)
      : OpResult_{OpResult::no_error}
      , Rights_{needsRights}
      , Path_{}
      , Root_{fairy.GetRootPath(OpResult_, seed, *const_cast<AclPath*>(&Path_), const_cast<AccessRight*>(&Rights_))}
      , ErrPos_{static_cast<size_t>(seed.begin() - Path_.begin())} {
   }
   SeedTicket(OpResult errn, StrView errmsg)
      : OpResult_{errn}
      , Path_{errmsg}
      , ErrPos_{errmsg.size() + 1} {
   }
};

/// \ingroup seed
/// 針對不同的操作, 要建立對應的 runner 處理, 跑完後通知 visitor.OnTicketRunnerDone();
class fon9_API TicketRunner : public SeedTicket, public SeedSearcher {
   fon9_NON_COPY_NON_MOVE(TicketRunner);
public:
   const SeedVisitorSP  Visitor_;
   /// 由使用者自訂的額外資訊.
   CharVector  Bookmark_;

   TicketRunner(SeedVisitor& visitor, StrView seed, AccessRight needsRights);
   TicketRunner(SeedVisitor& visitor, OpResult errn, const StrView& errmsg);

   void OnError(OpResult res) override;

   /// 在設定 this->RemainPath_.SetBegin(keyText.begin()); 之後,
   /// 將操作透過 op.Get(keyText, ...); 轉給 OnLastSeedOp(); 處理.
   void OnLastStep(TreeOp& op, StrView keyText, Tab& tab) override;

   virtual void OnLastSeedOp(const PodOpResult& resPod, PodOp* pod, Tab& tab) = 0;

   /// 起跑囉! 跑完後的結果(成功或失敗)透過 visitor.OnTicketRunnerDone(); 通知.
   void Run();
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_seed_SeedFairy_hpp__
