/// \file fon9/io/Server.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_Server_hpp__
#define __fon9_io_Server_hpp__
#include "fon9/io/Device.hpp"

namespace fon9 { namespace io {

/// \ingroup io
/// 提供 DeviceServer 接受 AcceptedClient 連入後, 建立一個 Session 給 AcceptedClient 使用.
class fon9_API SessionServer : public Session {
   fon9_NON_COPY_NON_MOVE(SessionServer);
public:
   using Session::Session;
   SessionServer() = default;
   /// 當 DeviceServer(例: TcpServer) **建立新 Device 之前** 的通知.
   /// 必須傳回一個 Session, 用來處理新進的連線.
   /// 可能在任意 thread 呼叫, 必須自行考慮 thread safe 的問題.
   virtual SessionSP OnDevice_Accepted(DeviceServer& dev) = 0;
};
using SessionServerSP = SessionSPT<SessionServer>;

/// \ingroup io
/// Server類型的通訊設備基底.
class fon9_API DeviceServer : public Device {
   fon9_NON_COPY_NON_MOVE(DeviceServer);
   using base = Device;
public:
   DeviceServer(SessionServerSP ses, ManagerSP mgr, Style style = Style::Server)
      : base(std::move(ses), std::move(mgr), style) {
   }
   /// Server 不支援, 傳回: std::errc::function_not_supported
   virtual SendResult SendASAP(const void* src, size_t size) override;
   virtual SendResult SendASAP(BufferList&& src) override;
   virtual SendResult SendBuffered(const void* src, size_t size) override;
   virtual SendResult SendBuffered(BufferList&& src) override;
   /// Server 不支援, 傳回 true;
   virtual bool IsSendBufferEmpty() const override;
   SessionSP OnDevice_Accepted() {
      return static_cast<SessionServer*>(this->Session_.get())->OnDevice_Accepted(*this);
   }
};

class fon9_API DeviceListener;
using DeviceListenerSP = intrusive_ptr<DeviceListener>;

/// \ingroup io
/// 由 DeviceServer/DeviceListener 接受連線之後, 填入 DeviceAcceptedClient 的序號.
/// 序號會回收再利用.
using DeviceAcceptedClientSeq = uint64_t;

/// \ingroup io
/// 透過 DeviceServer 連入的 Device 基底.
/// - 遠端連入後建立的對應 Device.
/// - 一旦斷線(LinkBroken, LinkError, Close...):
///   - 觸發 Dispose 最終會刪除對應的 Device.
///   - 告知 Listener 移除 this.
class fon9_API DeviceAcceptedClient : public Device {
   fon9_NON_COPY_NON_MOVE(DeviceAcceptedClient);
   using base = Device;

   friend DeviceListener;
   DeviceAcceptedClientSeq AcceptedClientSeq_{0};

protected:
   /// DeviceAcceptedClient 不可能執行到這兒, 因為是被動接受連線,
   /// 沒有「重新開啟」的操作, 所以這裡 do nothing.
   virtual void OpImpl_Reopen() override;

   /// 僅能在尚未 LinkReady 時執行, 一旦 LinkReady 就不會做任何事情!
   /// 在 Server 接受連線後, 會將 Device 設定好, 所以這裡的參數為 Server 提供的 deviceId.
   virtual void OpImpl_Open(std::string deviceId) override;

   /// 判斷: 如果是斷線, 則 this->Owner_->RemoveAcceptedClient(*this);
   virtual void OpImpl_StateChanged(const StateChangedArgs& e) override;

public:
   const DeviceListenerSP  Owner_;
   DeviceAcceptedClient(DeviceListenerSP owner, SessionSP ses, ManagerSP mgr, const DeviceOptions* optsDefault)
      : base(std::move(ses), std::move(mgr), Style::AcceptedClient, optsDefault)
      , Owner_{std::move(owner)} {
   }
   DeviceAcceptedClientSeq GetAcceptedClientSeq() const {
      return this->AcceptedClientSeq_;
   }
};

fon9_WARN_DISABLE_PADDING;
// 因為不確定: std::vector<DeviceAcceptedClientSP> 是否能最佳化.
// - 例如: erase(iter); 只用 memmove(); 就足夠.
// - 所以, 自行呼叫:
//   - intrusive_ptr_add_ref(): 在 AddAcceptedClient(); 之後呼叫.
//   - intrusive_ptr_release(): 在 Dispose(); 及 RemoveAcceptedClient(); 呼叫
struct fon9_API AcceptedClientsImpl : public std::vector<DeviceAcceptedClient*> {
   const_iterator find(DeviceAcceptedClientSeq seq) const;
   const_iterator lower_bound(DeviceAcceptedClientSeq seq) const;
};
fon9_API AcceptedClientsImpl::const_iterator ContainerLowerBound(const AcceptedClientsImpl& container, StrView strKeyText);
fon9_API AcceptedClientsImpl::const_iterator ContainerFind(const AcceptedClientsImpl& container, StrView strKeyText);
using AcceptedClients = MustLock<AcceptedClientsImpl>;

/// \ingroup io
/// 各類 Listener(例如: TcpListener) 的基底, 負責管理 DeviceAcceptedClient.
/// - DeviceServer 在更改設定時, 可能會建立新的 DeviceListener,
///   並且 Dispose 舊的 DeviceAcceptedClient.
///   舊的 DeviceListener 就會在 DeviceAcceptedClient 全部死亡時, 自然消失.
class fon9_API DeviceListener : public intrusive_ref_counter<DeviceListener> {
   fon9_NON_COPY_NON_MOVE(DeviceListener);
   friend DeviceAcceptedClient;

   bool  IsDisposing_{false};
   mutable AcceptedClients  AcceptedClients_;

   void RemoveAcceptedClient(DeviceAcceptedClient& devAccepted);
   
   /// acceptedClientSeqAndOthers = "seq cause...";
   /// 返回時 acceptedClientSeqAndOthers->begin() 會指向 seq 之後的第1個位置.
   DeviceSP GetAcceptedClient(StrView* acceptedClientSeqAndOthers);

   virtual void OnListener_Dispose() = 0;

protected:
   void AddAcceptedClient(DeviceServer& server, DeviceAcceptedClient& devAccepted, StrView connId);
   void SetAcceptedClientsReserved(size_t capacity) {
      if (capacity) {
         AcceptedClients::Locker devs{this->AcceptedClients_};
         devs->reserve(capacity);
      }
   }

public:
   DeviceListener() {
   }
   virtual ~DeviceListener();

   /// 提供給 IoManager 安全的取用 Device.ManagerBookmark.
   AcceptedClients::ConstLocker Lock() const{
      return AcceptedClients::ConstLocker{this->AcceptedClients_};
   }

   void Dispose(std::string cause);
   bool IsDisposing() const {
      return this->IsDisposing_;
   }

   size_t GetConnectionCount() const {
      AcceptedClients::Locker devs{this->AcceptedClients_};
      return devs->size();
   }

   void OpImpl_CloseAcceptedClient(StrView acceptedClientSeqAndOthers);
   void OpImpl_LingerCloseAcceptedClient(StrView acceptedClientSeqAndOthers);
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_io_Server_hpp__
