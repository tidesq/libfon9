/// \file fon9/auth/PolicyAgent.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_auth_PolicyAgent_hpp__
#define __fon9_auth_PolicyAgent_hpp__
#include "fon9/auth/PolicyTree.hpp"
#include "fon9/seed/MaTree.hpp"

namespace fon9 { namespace auth {

class fon9_API PolicyAgent : public seed::NamedSapling {
   fon9_NON_COPY_NON_MOVE(PolicyAgent);
   using base = seed::NamedSapling;

public:
   template <class... ArgsT>
   PolicyAgent(PolicyTreeSP tree, ArgsT&&... args)
      : base(std::move(tree), std::forward<ArgsT>(args)...) {
   }

   bool LinkStorage(InnDbf& storage, InnRoomSize minRoomSize) {
      return storage.LinkTable(&this->Name_, *static_cast<PolicyTree*>(this->Sapling_.get()), minRoomSize);
   }
};

} } // namespaces
#endif//__fon9_auth_PolicyAgent_hpp__
