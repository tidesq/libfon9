/// \file fon9/io/SocketClient.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_SocketClient_hpp__
#define __fon9_io_SocketClient_hpp__
#include "fon9/io/SocketConfig.hpp"
#include <vector>

#ifdef fon9_POSIX
#include <netdb.h>
#endif

namespace fon9 { namespace io {

fon9_WARN_DISABLE_PADDING;
/// \ingroup io
/// 一般用於 Tcp client 的 socket 參數.
/// 但也不一定是 Tcp client, 也可以是自訂協定(例: 使用 UDP 連到指定的遠端).
class fon9_API SocketClientConfig : public SocketConfig {
   using base = SocketConfig;
public:
   /// 使用 "dn=address:port" 設定(or "DN=").
   std::string DomainNames_;
   /// 使用 "Timeout=" 設定: 每次 dn解析、connect 逾時秒數.
   /// 預設=20
   unsigned    TimeoutSecs_;

   void SetDefaults();

   StrView ParseConfig(StrView  cfgstr, FnOnTagValue fnUnknownField);
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_io_SocketClient_hpp__
