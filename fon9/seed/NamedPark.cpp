/// \file fon9/seed/NamedPark.cpp
/// \author fonwinz@gmail.com
#include "fon9/seed/NamedPark.hpp"

namespace fon9 { namespace seed {

void ParkTree::OnMaTree_AfterRemove(Locker&, NamedSeed& seed) {
   this->Subj_.Publish(&seed, EventType::Remove);
}
void ParkTree::OnMaTree_AfterAdd(Locker&, NamedSeed& seed) {
   this->Subj_.Publish(&seed, EventType::Add);
}
void ParkTree::OnMaTree_AfterClear() {
   Locker locker{this->Container_};
   this->Subj_.Publish(nullptr, EventType::Clear);
}

} } // namespaces
