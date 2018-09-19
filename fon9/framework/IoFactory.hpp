/// \file fon9/framework/IoFactory.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_framework_IoFactory_hpp__
#define __fon9_framework_IoFactory_hpp__
#include "fon9/seed/NamedPark.hpp"
#include "fon9/io/Server.hpp"

namespace fon9 {

class fon9_API IoManager;
using IoManagerSP = intrusive_ptr<IoManager>;

enum class EnabledYN : char {
   /// 除了 Yes, 其餘皆為 No.
   Yes = 'Y'
};

template <class RevBuffer>
inline size_t ToBitv(RevBuffer& rbuf, EnabledYN value) {
   return ToBitv(rbuf, value == EnabledYN::Yes);
}

template <class RevBuffer>
inline void BitvTo(RevBuffer& rbuf, EnabledYN& value) {
   bool b = (value == EnabledYN::Yes);
   BitvTo(rbuf, b);
   value = b ? EnabledYN::Yes : EnabledYN{};
}

fon9_WARN_DISABLE_PADDING;
struct IoConfigItem {
   EnabledYN   Enabled_{};
   CharVector  SchArgs_;

   CharVector  SessionName_;
   CharVector  SessionArgs_;

   CharVector  DeviceName_;
   CharVector  DeviceArgs_;
};
fon9_WARN_POP;

//--------------------------------------------------------------------------//

class fon9_API SessionFactory : public seed::NamedSeed {
   fon9_NON_COPY_NON_MOVE(SessionFactory);
   using base = seed::NamedSeed;
public:
   using base::base;

   virtual io::SessionSP CreateSession(IoManager& mgr, const IoConfigItem& cfg, std::string& errReason) = 0;
   virtual io::SessionServerSP CreateSessionServer(IoManager& mgr, const IoConfigItem& cfg, std::string& errReason) = 0;
};
using SessionFactoryPark = seed::NamedPark<SessionFactory>;
using SessionFactoryParkSP = intrusive_ptr<SessionFactoryPark>;
using SessionFactorySP = SessionFactoryPark::ObjectSP;

//--------------------------------------------------------------------------//

class fon9_API DeviceFactory : public seed::NamedSeed {
   fon9_NON_COPY_NON_MOVE(DeviceFactory);
   using base = seed::NamedSeed;
public:
   using base::base;

   virtual io::DeviceSP CreateDevice(IoManagerSP mgr, SessionFactory& sesFactory, const IoConfigItem& cfg,
                                     std::string& errReason) = 0;
};
using DeviceFactoryPark = seed::NamedPark<DeviceFactory>;
using DeviceFactoryParkSP = intrusive_ptr<DeviceFactoryPark>;
using DeviceFactorySP = DeviceFactoryPark::ObjectSP;

//--------------------------------------------------------------------------//

fon9_API DeviceFactorySP GetIoFactoryTcpClient();
fon9_API DeviceFactorySP GetIoFactoryTcpServer();

} // namespaces
#endif//__fon9_framework_IoFactory_hpp__
