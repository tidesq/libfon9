/// \file fon9/io/SocketAddressDN.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_SocketAddressDN_hpp__
#define __fon9_io_SocketAddressDN_hpp__
#include "fon9/io/SocketAddress.hpp"
#include "fon9/Utility.hpp"
#include <vector>
#include <functional>

#ifdef fon9_POSIX
#include <netdb.h>
#endif

namespace fon9 { namespace io {

fon9_WARN_DISABLE_PADDING;
using SocketAddressList = std::vector<SocketAddress>;

/// \ingroup io
/// 透過 DomainNameParser::Parse() 查找 dn => address 的結果.
struct DomainNameParseResult {
   /// 解析成功取得的 address 加到尾端.
   SocketAddressList AddressList_;
   /// (dn1=eno:message),(dn3=eno:message)...
   std::string       ErrMsg_;

   void Clear() {
      this->AddressList_.clear();
      this->ErrMsg_.clear();
   }
};

/// \ingroup io
/// 把 domain names 解析成 ip address.
class fon9_API DomainNameParser {
   using port_t = SocketAddress::port_t;
   std::string DomainNames_; // 解析過程中, 此字串的內容會被改變!
   StrView     DnParser_;
   port_t      LastPortNo_;
   void Parse(StrView dnPort, DomainNameParseResult& res);
public:
   /// - 預設建構時會設定 AddrHints_.ai_family = AF_INET;
   /// - 若 AddrHints_.ai_family = AF_UNSPEC; 表示 AF_INET 或 AF_INET6 皆可.
   /// - 可自行修改後再進行 Parse()
   /// - Reset() 不會改變這裡的設定
   struct addrinfo   AddrHints_;

   /// 建構設定使用: 
   /// - this->AddrHints_.ai_family = AF_INET
   /// - this->AddrHints_.ai_socktype = SOCK_STREAM;
   DomainNameParser(int af = AF_INET, int type = SOCK_STREAM) {
      ZeroStruct(this->AddrHints_);
      this->AddrHints_.ai_family = af;
      this->AddrHints_.ai_socktype = type;
   }

   /// 重設解析內容.
   /// - dn 可用 ',' 分隔多個 domain names.
   /// - dn 若遇到 '[' 開頭, 則使用 IPv6 格式 "[domain name]:port"
   ///   - port 前面的 ':' 可省略
   ///   - port 必須在 "[]" 之後
   /// - 不會改變 AddrHints_
   void Reset(std::string dn);

   /// 一次取出一個用','分隔的 domain name, 透過 getaddrinfo() 取得 address, 加入 res.AddressList_.
   /// getaddrinfo() 為 blocking mode, 所以這裡可能會花一點時間!
   /// \retval false 已經沒有任何 domain name.
   /// \retval true  可能還有 domain name 需要解析.
   bool Parse(DomainNameParseResult& res);

   void ParseAll(DomainNameParseResult& res) {
      while (this->Parse(res)) {
      }
   }
};
fon9_WARN_POP;

using DnQueryReqId = uint64_t;
using FnOnSocketAddressList = std::function<void(DnQueryReqId id, DomainNameParseResult& res)>;

/// \ingroup io
/// - dn 可以包含多個 address:port, 使用 ',' 分隔即可.
/// - 在 fon9::GetDefaultThreadPool() 處理, 處理完畢透過 fnOnReady() 告知結果.
/// - id = 可丟給 AsyncDnQuery_Cancel() 取消.
/// - 可能在返回前就呼叫了 fnOnReady(); 但在呼叫 fnOnReady() 之前 id 必定已經填妥.
fon9_API void AsyncDnQuery(DnQueryReqId& id, std::string dn, FnOnSocketAddressList fnOnReady);

/// 若要取消的id正在呼叫 fnOnReady(), 則會等 fnOnReady() 結束時才返回.
fon9_API void AsyncDnQuery_CancelAndWait(DnQueryReqId id);
fon9_API void AsyncDnQuery_Cancel(DnQueryReqId id);

} } // namespaces
#endif//__fon9_io_SocketAddressDN_hpp__
