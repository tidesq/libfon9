/// \file fon9/seed/PodOp.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_seed_PodOp_hpp__
#define __fon9_seed_PodOp_hpp__
#include "fon9/seed/TreeOp.hpp"

namespace fon9 { namespace seed {

fon9_WARN_DISABLE_PADDING;
struct SeedOpResult : public PodOpResult {
   SeedOpResult(Tree& sender, OpResult res, const StrView& key, Tab* tab = nullptr)
      : PodOpResult{sender, res, key}
      , Tab_{tab} {
   }
   /// - 在 FnCommandResultHandler() 裡面:
   ///   - 若為 nullptr 表示 Pod Command 的結果.
   ///   - 若為 !nullptr 表示 Seed Command的結果.
   /// - 在 FnReadOp() 或 FnWriteOp()
   ///   - 此值必定有效, 不會是 nullptr.
   Tab*     Tab_;
};
fon9_WARN_POP;

using FnReadOp = std::function<void(const SeedOpResult& res, const RawRd* rd)>;
using FnWriteOp = std::function<void(const SeedOpResult& res, const RawWr* wr)>;
using FnCommandResultHandler = std::function<void(const SeedOpResult& res, StrView msg)>;

//--------------------------------------------------------------------------//

class fon9_API PodOp {
   fon9_NON_COPY_NON_MOVE(PodOp);
public:
   PodOp() = default;

   /// 取得現有的 Sapling, 若不存在或尚未建立, 則傳回 nullptr.
   virtual TreeSP GetSapling(Tab& tab);

   /// 若不存在則嘗試建立, 否則傳回現有的 Sapling.
   /// 預設: 直接呼叫 this->GetSapling(tab);
   virtual TreeSP MakeSapling(Tab& tab);

   /// 對此 Pod(or Seed) 進行指令操作.
   /// 不一定會在返回前呼叫 resHandler, 也不一定會在同一個 thread 呼叫 resHandler, 由衍生者決定.
   virtual void OnSeedCommand(Tab* tab, StrView cmdln, FnCommandResultHandler resHandler) = 0;

   virtual void BeginRead(Tab& tab, FnReadOp fnCallback) = 0;
   virtual void BeginWrite(Tab& tab, FnWriteOp fnCallback) = 0;
};

//--------------------------------------------------------------------------//

class fon9_API PodOpDefault : public PodOp, public SeedOpResult {
   fon9_NON_COPY_NON_MOVE(PodOpDefault);
public:
   using SeedOpResult::SeedOpResult;

   /// 對此 Seed 進行指令操作.
   /// 不一定會在返回前呼叫 resHandler, 也不一定會在同一個 thread 呼叫 resHandler, 由衍生者決定.
   /// 預設: 直接呼叫 resHandler(OpResult::not_supported_cmd, cmdpr);
   void OnSeedCommand(Tab* tab, StrView cmdln, FnCommandResultHandler resHandler) override;

   void BeginRead(Tab& tab, FnReadOp fnCallback) override;
   void BeginWrite(Tab& tab, FnWriteOp fnCallback) override;

   template <class FnOp, class RawRW>
   void BeginRW(Tab& tab, FnOp&& fnCallback, RawRW&& op) {
      assert(this->Sender_->LayoutSP_->GetTab(static_cast<size_t>(tab.GetIndex())) == &tab);
      this->Tab_ = &tab;
      this->OpResult_ = OpResult::no_error;
      fnCallback(*this, &op);
   }
};

template <class Pod>
class PodOpReadonly : public PodOpDefault {
   fon9_NON_COPY_NON_MOVE(PodOpReadonly);
   using base = PodOpDefault;
public:
   Pod&  Pod_;
   PodOpReadonly(Pod& pod, Tree& sender, const StrView& key)
      : base{sender, OpResult::no_error, key}
      , Pod_(pod) {
   }
   void BeginRead(Tab& tab, FnReadOp fnCallback) override {
      this->BeginRW(tab, std::move(fnCallback), SimpleRawRd{this->Pod_});
   }
};

} } // namespaces
#endif//__fon9_seed_PodOp_hpp__
