/// \file fon9/io/SocketConfig.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_SocketConfig_hpp__
#define __fon9_io_SocketConfig_hpp__
#include "fon9/io/IoBase.hpp"
#include "fon9/io/SocketAddress.hpp"

namespace fon9 { namespace io {

using FnOnTagValue = std::function<bool(StrView tag, StrView value)>;

template <class DeviceT>
inline static bool OpThr_ParseDeviceConfig(DeviceT& dev, StrView cfgstr, FnOnTagValue fnUnknownField) {
   dev.OpThr_SetState(State::Opening, cfgstr);
   dev.Config_.SetDefaults();
   StrView errfn = dev.Config_.ParseConfig(cfgstr, std::move(fnUnknownField));
   if (errfn.empty())
      return true;
   std::string  errmsg;
   errmsg.assign("Unknown config: {");
   errfn.AppendTo(errmsg);
   errmsg.push_back('}');
   dev.OpThr_SetState(State::ConfigError, &errmsg);
   return false;
}

fon9_WARN_DISABLE_PADDING;
/// \ingroup io
/// Socket 基本參數.
struct fon9_API SocketOptions {
   /// 使用 "TcpNoDelay=N" 關閉 TCP_NODELAY.
   /// 預設為 Y.
   int   TCP_NODELAY_;
   /// 使用 "SNDBUF=size" 設定.
   /// SO_SNDBUF_ < 0: 使用系統預設值, 否則使用 SO_SNDBUF_ 的設定.
   int   SO_SNDBUF_;
   /// 使用 "RCVBUF=size" 設定.
   /// SO_RCVBUF_ < 0: 使用系統預設值, 否則使用 SO_RCVBUF_ 的設定.
   int   SO_RCVBUF_;
   /// 使用 "ReuseAddr=Y" 設定: 請參閱 SO_REUSEADDR 的說明.
   int   SO_REUSEADDR_;
   /// 使用 "ReusePort=Y" 設定: 請參閱 SO_REUSEPORT 的說明 (Windows沒有此選項).
   int   SO_REUSEPORT_;

   /// 請參閱 SO_LINGER 的說明.
   /// 使用 "Linger=N" 設定(開啟linger,但等候0秒): Linger_.l_onoff=1; Linger_.l_linger=0; 避免 TIME_WAIT.
   /// |Linger_.l_onoff | Linger_.l_linger                                        |
   /// |:--------------:|:--------------------------------------------------------|
   /// |   0            | 系統預設, close()立即返回, 但仍盡可能將剩餘資料送給對方.|
   /// |   非0          | 0: 放棄未送出的資料,並送RST給對方,可避免 TIME_WAIT 狀態 |
   /// |   非0          | 非0: close()等候秒數,或所有資料已送給對方,且正常結束連線|
   struct linger  Linger_;

   /// 使用 "KeepAlive=n" 設定.
   /// - <=0: 不使用 KEEPALIVE 選項(預設).
   /// - ==1: 使用 SO_KEEPALIVE 設為 true, 其餘(間隔時間)為系統預設值.
   /// - >1:  TCP_KEEPIDLE,TCP_KEEPINTVL 的間隔秒數, 此時 TCP_KEEPCNT 一律設為 3.
   int KeepAliveInterval_;

   /// 全部設為預設值.
   void SetDefaults();

   StrView ParseConfig(StrView cfgstr, FnOnTagValue fnUnknownField);
};

/// \ingroup io
/// Socket 設定.
struct fon9_API SocketConfig {
   /// 使用 "Bind=address:port" or "Bind=port" 設定: 綁定的 local address.
   /// - Tcp Server: listen address & port.
   /// - Tcp Client: local address & port.
   SocketAddress AddrBind_;
   /// 使用 "Remote=address:port" 設定: 目的地位置.
   /// - Tcp Server: 一般無此參數, 若有提供[專用Server]則可設定此參數, 收到連入連線後判斷是否是正確的來源.
   /// - Tcp Client: 遠端Server的 address & port.
   SocketAddress AddrRemote_;
   SocketOptions Options_;

   /// 取得 Address family: 透過 AddrBind_ 的 sa_family.
   AddressFamily GetAF() const {
      return static_cast<AddressFamily>(AddrBind_.Addr_.sa_family);
   }

   /// 全部設為預設值.
   void SetDefaults();

   /// 根據解析的設定, 填入各項參數, 各項參數間使用 '|' 分隔.
   /// - 第一個分隔之前, 若沒有「=」, 則將該值填入 addrDefault
   /// - 設定之前 **不會** 先設定為預設值, 您可以用 SetDefaults() 先設定預設, 並自行先調整預設值.
   /// - 透過 ParseField() 解析設定.
   /// - 若遇到不認識的 field, 則會透過 fnUnknownField(tag,value) 來處理.
   /// \return retval.empty(): 解析正確; 否則為: 第一個發生錯誤的 "tag" 或 "tag=value"
   StrView ParseConfig(StrView cfgstr, SocketAddress* addrDefault, FnOnTagValue fnUnknownField);

   /// TcpClient: cfgstr = "ip:port|...opts(tag=value)..."
   /// this->AddrRemote_ = ip:port;
   StrView ParseConfigClient(StrView cfgstr, FnOnTagValue fnUnknownField) {
      return this->ParseConfig(cfgstr, &this->AddrRemote_, std::move(fnUnknownField));
   }

   /// TcpServer: cfgstr = "port|...opts(tag=value)..."
   /// this->AddrBind_ = any:port;
   StrView ParseConfigListen(StrView cfgstr, FnOnTagValue fnUnknownField) {
      return this->ParseConfig(cfgstr, &this->AddrBind_, std::move(fnUnknownField));
   }
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_io_SocketConfig_hpp__
