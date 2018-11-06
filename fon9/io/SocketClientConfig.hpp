/// \file fon9/io/SocketClientConfig.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_SocketClientConfig_hpp__
#define __fon9_io_SocketClientConfig_hpp__
#include "fon9/io/SocketConfig.hpp"

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
   using BaseParser = base::Parser;
public:
   /// 使用 AsyncDnQuery() 解析 DomainNames_, 所以可以包含多個 address:port, 使用 ',' 分隔即可.
   std::string DomainNames_;

   /// 每次 dn解析、connect 逾時秒數, 預設=20
   unsigned    TimeoutSecs_;

   void SetDefaults();

   struct fon9_API Parser : public BaseParser {
      fon9_NON_COPY_NON_MOVE(Parser);
      /// 若設定字串為 "ip:port|...opts(tag=value)...", 則 this->AddrRemote_ = ip:port;
      Parser(SocketClientConfig& owner) : BaseParser{owner, owner.AddrRemote_} {
      }
      /// - "dn=" (or "DN=") 設定 this->DomainNames_
      /// - "Timeout=" 設定 this->TimeoutSecs_
      /// - 其餘使用 BaseParser::OnTagValue(StrView tag, StrView& value); 處理.
      Result OnTagValue(StrView tag, StrView& value) override;
   };
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_io_SocketClientConfig_hpp__
