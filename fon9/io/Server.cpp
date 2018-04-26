/// \file fon9/io/Server.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/Server.hpp"

namespace fon9 { namespace io {

bool DeviceServer::IsSendBufferEmpty() const {
   return true;
}
Device::SendResult DeviceServer::SendASAP(const void* src, size_t size) {
   (void)src; (void)size;
   return SendResult{std::errc::function_not_supported};
}
Device::SendResult DeviceServer::SendASAP(BufferList&& src) {
   BufferListConsumeErr(std::move(src), std::errc::function_not_supported);
   return SendResult{std::errc::function_not_supported};
}
Device::SendResult DeviceServer::SendBuffered(const void* src, size_t size) {
   (void)src; (void)size;
   return SendResult{std::errc::function_not_supported};
}
Device::SendResult DeviceServer::SendBuffered(BufferList&& src) {
   BufferListConsumeErr(std::move(src), std::errc::function_not_supported);
   return SendResult{std::errc::function_not_supported};
}

} } // namespaces
