/// \file fon9/io/SocketConfig.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_SocketConfig_hpp__
#define __fon9_io_SocketConfig_hpp__
#include "fon9/io/IoBase.hpp"
#include "fon9/io/SocketAddress.hpp"

namespace fon9 { namespace io {

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

   void SetDefaults();

   ConfigParser::Result OnTagValue(StrView tag, StrView& value);
};

/// \ingroup io
/// Socket 設定.
struct fon9_API SocketConfig {
   /// 綁定的 local address.
   /// - Tcp Server: listen address & port.
   /// - Tcp Client: local address & port.
   SocketAddress AddrBind_;
   /// 目的地位置.
   /// - Tcp Server: 一般無此參數, 若有提供[專用Server]則可設定此參數, 收到連入連線後判斷是否是正確的來源.
   /// - Tcp Client: 遠端Server的 address & port.
   SocketAddress AddrRemote_;
   SocketOptions Options_;

   /// 取得 Address family: 透過 AddrBind_ 的 sa_family.
   AddressFamily GetAF() const {
      return static_cast<AddressFamily>(AddrBind_.Addr_.sa_family);
   }

   void SetDefaults();

   /// - 如果沒有設定 AddrBind_ family 則使用 AddrRemote_ 的 sa_family;
   /// - 如果沒有設定 AddrRemote_ family 則使用 AddrBind_ 的 sa_family;
   void SetAddrFamily();

   /// - "Bind=address:port" or "Bind=port" 
   /// - "Remote=address:port"
   /// - 其餘轉給 this->Options_ 處理.
   ConfigParser::Result OnTagValue(StrView tag, StrView& value);

   struct fon9_API Parser : public ConfigParser {
      fon9_NON_COPY_NON_MOVE(Parser);
      SocketConfig&  Owner_;
      SocketAddress* AddrDefault_;
      Parser(SocketConfig& owner, SocketAddress& addrDefault)
         : Owner_(owner)
         , AddrDefault_{&addrDefault} {
      }
      /// 解構時呼叫 this->Owner_.SetAddrFamily();
      ~Parser();

      /// - 第一個沒有「=」的欄位(value.begin()==nullptr): 將該值填入 *this->AddrDefault_;
      ///   然後將 this->AddrDefault_ = nullptr;
      ///   也就是只有第一個沒有「=」的欄位會填入 *this->AddrDefault_, 其餘視為錯誤.
      /// - 其餘轉給 this->Owner_.OnTagValue() 處理.
      Result OnTagValue(StrView tag, StrView& value) override;
   };
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_io_SocketConfig_hpp__
