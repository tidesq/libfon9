/// \file fon9/io/Device.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_Device_hpp__
#define __fon9_io_Device_hpp__
#include "fon9/io/Manager.hpp"
#include "fon9/io/Session.hpp"
#include "fon9/io/DeviceAsyncOp.hpp"
#include "fon9/Outcome.hpp"
#include "fon9/Timer.hpp"

namespace fon9 { namespace io {

using Bookmark = uintmax_t;

/// \ingroup io
/// 通訊設備基底.
class fon9_API Device : public intrusive_ref_counter<Device> {
   fon9_NON_COPY_NON_MOVE(Device);

   State          State_;
   Bookmark       SessionBookmark_{0};
   Bookmark       ManagerBookmark_{0};
   std::string    DeviceId_;
   DeviceOptions  Options_;
   
public:
   const Style       Style_;
   const ManagerSP   Manager_;
   // 有時 Session_ 會參考用到 Manager_,
   // 所以 Session_ 必須比 Manager_ 先死, 因此必須放在 Manager_ 之後.
   const SessionSP   Session_;

   /// - 建構者必須在建構完畢, 取得一個 DeviceSP 之後, 立即呼叫 Initialize();
   /// - 必須提供有效的 ses, 運行過程中不會再檢查 this->Session_ 是否有效.
   /// - mgr 可以為 nullptr.
   Device(SessionSP ses, ManagerSP mgr, Style style, const DeviceOptions* optsDefault = nullptr)
      : State_{State::Initializing}
      , Style_{style}
      , Manager_{std::move(mgr)}
      , Session_{std::move(ses)}
      , CommonTimer_{GetDefaultTimerThread()} {
      assert(this->Session_);
      if (optsDefault)
         this->Options_ = *optsDefault;
   }

   /// 初始化 Device, 返回後才能呼叫 AsyncOpen() 及其他 methods.
   /// 預設:
   /// - 觸發 Session::OnDevice_Initialized() 事件.
   /// - 觸發 Manager::OnDevice_Initialized() 事件.
   /// - 設定 State::Initialized, 但不會觸發 OnDevice_StateChanged() 事件.
   virtual void Initialize();

   virtual ~Device();

   void SetSessionBookmark(Bookmark n) { this->SessionBookmark_ = n; }
   Bookmark GetSessionBookmark() const { return this->SessionBookmark_; }
   void SetManagerBookmark(Bookmark n) { this->ManagerBookmark_ = n; }
   Bookmark GetManagerBookmark() const { return this->ManagerBookmark_; }

   /// - 如果 !cfgstr.empty() 則使用 OpImpl_Open(cfgstr) 來開啟.
   /// - 如果 cfgstr.empty()  會先檢查現在狀態是否允許 reopen, 如果可以, 則使用 OpImpl_Reopen() 來開啟.
   void AsyncOpen(std::string cfgstr) {
      this->OpQueue_.AddTask(DeviceAsyncOp(&Device::OpThr_Open, std::move(cfgstr)));
   }
   void AsyncClose(std::string cause) {
      this->OpQueue_.AddTask(DeviceAsyncOp(&Device::OpThr_Close, std::move(cause)));
   }
   /// 等到 this->IsSendBufferEmpty()==true 時關閉 device.
   void AsyncLingerClose(std::string cause) {
      this->OpQueue_.AddTask(DeviceAsyncOp(&Device::OpThr_LingerClose, std::move(cause)));
   }
   void AsyncCheckLingerClose(DeviceOpQueue::ALockerBase& alocker) {
      if (fon9_UNLIKELY(this->OpImpl_GetState() == State::Lingering))
         alocker.AddAsyncTask(DeviceAsyncOp(&Device::OpThr_CheckLingerClose, std::string{}));
   }
   static void OpThr_CheckLingerClose(Device& dev, std::string cause);

   /// 到 OpThr_Dispose() 處理:
   /// - OpThr_Close(cause):
   ///    - 如果現在狀態 < State::Closing; 會先呼叫 this->OpImpl_Close(cause);
   /// - OpImpl_SetState(State::Disposing, cause):
   ///   - 如果成功進入 State::Disposing 狀態, 才會呼叫 this->OpImpl_Dispose(cause);
   void AsyncDispose(std::string cause) {
      this->OpQueue_.AddTask(DeviceAsyncOp(&Device::OpThr_Dispose, std::move(cause)));
   }

   /// 判斷傳送緩衝區是否為空(已無等候送出的資料), 可在任意 thread 呼叫.
   /// 衍生者實作時須注意 thread safe.
   virtual bool IsSendBufferEmpty() const = 0;

   using SendResult = Outcome<size_t>;
   /// **盡可能立即** 把資料送出.
   /// 只有在 State::LinkReady 的狀態下才可送資料.
   /// \retval Success 立即送出的資料量(可能為0), 可能有部分或全部立即送出, 未立即送出的會放進 buffer 等候送出.
   /// \retval std::errc::no_link  狀態不是 State::LinkReady 無法傳送.
   virtual SendResult SendASAP(const void* src, size_t size) = 0;
   virtual SendResult SendASAP(BufferList&& src) = 0;
   template <class StrT>
   SendResult StrSendASAP(const StrT& str) {
      return this->SendASAP(&*str.begin(), str.size());
   }
   /// 把資料送出, 但 **盡快返回**.
   /// - 可能的方法是: 把資料放到 buffer, 然後通知 傳送thread 送出資料.
   virtual SendResult SendBuffered(const void* src, size_t size) = 0;
   virtual SendResult SendBuffered(BufferList&& src) = 0;
   template <class StrT>
   SendResult StrSendBuffered(const StrT& str) {
      return this->SendBuffered(&*str.begin(), str.size());
   }

   SendResult Send(const void* src, size_t size) {
      return IsEnumContains(this->Options_.Flags_, DeviceFlag::SendASAP)
         ? this->SendASAP(src, size)
         : this->SendBuffered(src, size);
   }
   SendResult Send(BufferList&& src) {
      return IsEnumContains(this->Options_.Flags_, DeviceFlag::SendASAP)
         ? this->SendASAP(std::move(src))
         : this->SendBuffered(std::move(src));
   }
   template <class StrT>
   SendResult StrSend(const StrT& str) {
      return this->Send(&*str.begin(), str.size());
   }

   /// 到 Op thread 取得 DeviceId, 然後才返回.
   /// - 如果是在 OnDevice_* 事件(除了: OnDevice_Initialized(), OnDevice_Destructing()) 裡呼叫,
   ///   表示已在 Op thread, 則會立即取得&返回.
   /// - 因為「取得 DeviceId」不是常用操作, 所以不考慮速度.
   /// - 由於可能會進入等待, 所以呼叫前不應有任何 lock, 須謹慎考慮死結問題.
   std::string WaitGetDeviceId();

   /// 取得 device 的現在狀態訊息字串.
   /// - |tm=ToStrRev_Full(UtcNow())
   /// - |st=GetStateStr(State_)
   /// - |id={DeivceId_}
   /// - |info=其他訊息(透過 OpImpl_AppendDeviceInfo() 取得)
   ///
   /// - DeviceId_ 前後有大括號.
   std::string WaitGetDeviceInfo();

   /// 設定 device 的屬性參數, Device 支援:
   /// - SendASAP=N  預設值為 'Y'，只要不是 'N' 就會設定成 Yes(若未設定，初始值為 Yes)。
   /// - RetryInterval=n   LinkError 之後重新嘗試的延遲時間, 預設值為 15 秒, 0=不要 retry.
   /// - ReopenInterval=n  LinkBroken 或 ListenBroken 之後, 重新嘗試的延遲時間, 預設值為 3 秒, 0=不要 reopen.
   ///   使用 TimeInterval 格式設定, 延遲最小單位為 ms, e.g.
   ///   "RetryInterval=3"    表示連線失敗後, 延遲  3 秒後重新連線.
   ///   "ReopenInterval=0.5" 表示斷線後, 延遲  0.5 秒後重新連線.
   std::string WaitSetProperty(StrView strTagValueList);

   /// 取得現在狀態, 必須在 op safe 狀態下才能確保正確, 否則僅供參考.
   State OpImpl_GetState() const {
      return this->State_;
   }
   const DeviceOptions&  OpImpl_GetOptions() const {
      return this->Options_;
   }

   /// - Device 本身會針對 LinkError, LinkBroken, ListenBroken 啟動 Timer, 呼叫 Reopen()
   /// - 在 LinkReady 時, 提供 Session 使用, 透過 OnDevice_CommonTimer(); 通知.
   /// - 其他情況提供衍生者使用.
   void CommonTimerRunAfter(TimeInterval ti) {
      this->CommonTimer_.RunAfter(ti);
   }

   /// 執行特定命令.
   /// 可在任意 thread 呼叫此處.
   /// - open    cfgstr       this->AsyncOpen(cfgstr);
   /// - close   cause        this->AsyncClose(cause);
   /// - lclose  cause        this->AsyncLingerClose(cause);
   /// - dispose cause        this->AsyncDispose(cause);
   /// - info                 return this->WaitGetDeviceInfo();
   /// - set     tag=value    return this->WaitSetProperty(list of "tag=value|tag2=value2");
   /// - ses     cmd          return this->Session_->SessionCommand(cmd);
   /// - 其餘傳回 "unknown device command"
   std::string DeviceCommand(StrView cmdln);

   static Device& StaticCast(DeviceOpQueue& opQueue) {
      return ContainerOf(opQueue, &Device::OpQueue_);
   }
   /// 為了讓輔助類別(函式), 能順利、安全的操作 Device,
   /// 所以將 OpQueue_ 放在 public.
   mutable DeviceOpQueue  OpQueue_;

protected:
   /// 預設 do nothing. 提供給衍生者使用.
   virtual void OnCommonTimer(TimeStamp now);
   static void EmitOnCommonTimer(TimerEntry*, TimeStamp now);
   using CommonTimer = DataMemberEmitOnTimer<&EmitOnCommonTimer>;
   CommonTimer CommonTimer_;

   friend struct DeviceAsyncOpInvoker;

   /// 預設使用 GetDefaultThreadPool() 執行 OpQueue_.
   virtual void MakeCallForWork();

   // OpThr 或 OpImpl 開頭的函式, 都必須在 OpQueue_ thread 裡面呼叫:
   // - OpThr 通常為 OpQueue_.AddTask() 的進入點, 在檢查狀態後, 再決定如何呼叫 OpImpl.
   // - OpImpl 皆為 virtual, 交給衍生者實作.

   /// - 透過 OpThr_Open() 而來的呼叫, 必定: !cfgstr.empty(); && st<State::Disposing
   virtual void OpImpl_Open(std::string cfgstr) = 0;
   /// 透過 OpThr_Open() 而來的呼叫, 此時必定:
   /// - st < State::Disposing && st != State::ConfigError && st != Ready(LinkReady,Listening,WaitingLinkIn)
   virtual void OpImpl_Reopen() = 0;
   /// 關閉完畢後, 必須由衍生者設定 State::Closed 狀態.
   /// 若要花些時間關閉(例:等候遠端確認), 則可先設定 State::Closing 狀態.
   virtual void OpImpl_Close(std::string cause) = 0;
   /// 呼叫此處時, 狀態必定為 State::Disposing; 預設 do nothing.
   virtual void OpImpl_Dispose(std::string cause);

   /// 狀態異動通知衍生者.
   /// 在觸發 this->Session_->OnDevice_StateChanged(*this, e); 之前，告知衍生者。
   /// 預設 do nothing.
   virtual void OpImpl_StateChanged(const StateChangedArgs& e);
   virtual void OpImpl_AppendDeviceInfo(std::string& info);

   /// 現在狀態必須 < State::Disposing; 才允許呼叫 this->OpImpl_Open(cfgstr);
   static void OpThr_Open(Device& dev, std::string cfgstr);
   /// 現在狀態必須 < State::Closing; 才需要呼叫 this->OpImpl_Close(cause);
   static void OpThr_Close(Device& dev, std::string cause);
   static void OpThr_LingerClose(Device& dev, std::string cause);

   /// Dispose 正常程序: 先 Close, 再處理 Dispose:
   /// `OpThr_Close(dev, cause); OpThr_DisposeNoClose(dev, std::move(cause));`
   static void OpThr_Dispose(Device& dev, std::string cause);
   /// Dispose 強制程序: 直接處理 Dispose, 沒有先呼叫 Close.
   /// if (dev.OpImpl_SetState(State::Disposing, &cause))
   ///    dev.OpImpl_Dispose(std::move(cause));
   static void OpThr_DisposeNoClose(Device& dev, std::string cause);

   /// 每個 Device 在 LinkReady 時(有些在建構時,有些在Open成功時), 都有一個識別用的Id,
   /// - 例: TcpClient: "R=RemoteIp:Port|L=LocalIp:Port"
   /// - 由於 WaitGetDeviceInfo() 會填入: "|id={DeivceId_}"; 所以 deviceId 不可有沒配對的大括號.
   static void OpThr_SetDeviceId(Device& dev, std::string deviceId) {
      dev.DeviceId_ = std::move(deviceId);
   }
   const std::string& OpImpl_GetDeviceId() const {
      return this->DeviceId_;
   }
   /// \retval true  將 this->State_ 設為 afst,
   ///               並觸發 Session::OnDevice_StateChanged(), Manager::OnDevice_StateChanged() 事件.
   /// \retval false 狀態不須變動(this->State_==afst): 觸發 OnDevice_StateUpdated();
   ///               或無法變動(已進入 Disposing 程序), 不會觸發任何事件.
   bool OpImpl_SetState(State afst, StrView stmsg);

   /// OpThr_SetLinkReady() 的流程:
   /// - 衍生者: 發現 LinkReady, 進入 Op thread 呼叫 OpThr_SetLinkReady();
   /// - 在 OpThr_SetLinkReady() 裡面:
   ///   - 衍生者如果要「得知 LinkReady 狀態」應在 OpImpl_StateChanged() 裡面處理:
   ///      \code
   ///         virtual void OpImpl_StateChanged(const StateChangedArgs& e) override {
   ///            if (e.After_ == State::LinkReady)
   ///               ... do something ...;
   ///            base::OpImpl_StateChanged(e);
   ///         }
   ///      \endcode
   ///   - this->OpImpl_SetState(State::LinkReady, stmsg);
   ///   - if (this->State_ == State::LinkReady)
   ///      this->OpImpl_StartRecv(this->Session_->OnDevice_LinkReady(*this));
   static void OpThr_SetLinkReady(Device& dev, std::string stmsg);
   /// preallocSize = this->Session_->OnDevice_LinkReady(*this);
   virtual void OpImpl_StartRecv(RecvBufferSize preallocSize);

   static void OpThr_SetBrokenState(Device& dev, std::string cause);
   void AsyncSetBrokenState(std::string cause) {
      this->OpQueue_.AddTask(DeviceAsyncOp(&Device::OpThr_SetBrokenState, std::move(cause)));
   }

   /// \copydoc DeviceCommand()
   virtual std::string OnDevice_Command(StrView cmd, StrView param);

   /// 可能透過 WaitSetProperty() 呼叫到此處.
   virtual ConfigParser::Result OpImpl_SetProperty(StrView tag, StrView& value);

   void SetNoRecvEvent() {
      this->Options_.Flags_ |= DeviceFlag::NoRecvEvent;
   }
};

//--------------------------------------------------------------------------//

inline void DeviceAsyncOpInvoker::MakeCallForWork() {
   Device&  owner = ContainerOf(DeviceOpQueue::StaticCast(*this), &Device::OpQueue_);
   owner.MakeCallForWork();
}
inline void DeviceAsyncOpInvoker::Invoke(DeviceAsyncOp& task) {
   Device& dev = ContainerOf(DeviceOpQueue::StaticCast(*this), &Device::OpQueue_);
   if (task.FnAsync_)
      (*task.FnAsync_)(dev, std::move(task.FnAsyncArg_));
   else
      task.AsyncTask_(dev);
}

inline void DeviceOpLocker::Create(Device& dev, AQueueTaskKind taskKind) {
   this->ALocker_.emplace(dev.OpQueue_, taskKind);
}
inline Device& DeviceOpLocker::GetDevice() const {
   return Device::StaticCast(this->ALocker_.get()->Owner_);
}

} } // namespaces
#endif//__fon9_io_Device_hpp__
