/// \file fon9/seed/Tree.cpp
/// \author fonwinz@gmail.com
#include "fon9/seed/Tree.hpp"

namespace fon9 { namespace seed {

Tree::~Tree() {
}

void Tree::OnTreeReleased() {
   if (this->use_count() == 0)
      delete this;
}

void Tree::OnTreeOp(FnTreeOp fnCallback) {
   fnCallback(TreeOpResult{this, OpResult::not_supported_tree_op}, nullptr);
}

void Tree::OnTabTreeOp(FnTreeOp fnCallback) {
   fnCallback(TreeOpResult{this, OpResult::not_supported_tree_op}, nullptr);
}

void Tree::OnParentSeedClear() {
}

} } // namespaces
