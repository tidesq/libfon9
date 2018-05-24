/// \file fon9/io/Session.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/Session.hpp"

namespace fon9 { namespace io {

Session::~Session() {
}
void Session::OnDevice_Initialized(Device& dev) {
   (void)dev;
}
void Session::OnDevice_Destructing(Device& dev) {
   (void)dev;
}
void Session::OnDevice_StateChanged(Device& dev, const StateChangedArgs& e) {
   (void)dev; (void)e;
}
void Session::OnDevice_StateUpdated(Device& dev, const StateUpdatedArgs& e) {
   (void)dev; (void)e;
}
RecvBufferSize Session::OnDevice_LinkReady(Device& dev) {
   (void)dev;
   return RecvBufferSize::Default;
}
RecvBufferSize Session::OnDevice_Recv(Device& dev, DcQueueList& rxbuf) {
   (void)dev; (void)rxbuf;
   return RecvBufferSize::Default;
}
void Session::OnDevice_CommonTimer(Device& dev, TimeStamp now) {
   (void)dev; (void)now;
}
//void Session::OnDevice_SendBufferEmpty(Device& dev) {
//   (void)dev;
//}
std::string Session::SessionCommand(Device& dev, StrView cmdln) {
   (void)dev; (void)cmdln;
   return "unknown session command";
}

} } // namespaces
