/// \file fon9/seed/PodOp.cpp
/// \author fonwinz@gmail.com
#include "fon9/seed/PodOp.hpp"
#include "fon9/seed/Tree.hpp"
#include "fon9/RevPut.hpp"

namespace fon9 { namespace seed {

TreeSP PodOp::MakeSapling(Tab& tab) {
   return this->GetSapling(tab);
}
TreeSP PodOp::GetSapling(Tab& /*tab*/) {
   return TreeSP{};
}

//--------------------------------------------------------------------------//

void PodOpDefault::OnSeedCommand(Tab* tab, StrView /*cmd*/, FnCommandResultHandler resHandler) {
   this->OpResult_ = OpResult::not_supported_cmd;
   this->Tab_ = tab;
   resHandler(*this, nullptr);
}
void PodOpDefault::BeginRead(Tab& tab, FnReadOp fnCallback) {
   this->OpResult_ = OpResult::not_supported_read;
   this->Tab_ = &tab;
   fnCallback(*this, nullptr);
}
void PodOpDefault::BeginWrite(Tab& tab, FnWriteOp fnCallback) {
   this->OpResult_ = OpResult::not_supported_write;
   this->Tab_ = &tab;
   fnCallback(*this, nullptr);
}

//--------------------------------------------------------------------------//

fon9_API void FieldsCellRevPrint(const Fields& fields, const RawRd& rd, RevBuffer& rbuf, char chSplitter) {
   if (size_t fldidx = fields.size()) {
      while (const Field* fld = fields.Get(--fldidx)) {
         fld->CellRevPrint(rd, nullptr, rbuf);
         RevPutChar(rbuf, chSplitter);
      }
   }
}

} } // namespaces
