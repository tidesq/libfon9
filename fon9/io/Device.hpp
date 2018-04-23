/// \file fon9/io/Device.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_Device_hpp__
#define __fon9_io_Device_hpp__
#include "fon9/io/Manager.hpp"
#include "fon9/io/Session.hpp"
#include "fon9/io/DeviceAsyncOp.hpp"
#include "fon9/Outcome.hpp"

namespace fon9 { namespace io {

using Bookmark = uintmax_t;

enum class DeviceCommonOption {
   SendASAP = 0x01,
   /// 若 `RecvBufferSize OnDevice_LinkReady();` 或 `RecvBufferSize OnDevice_Recv();`
   /// 傳回 RecvBufferSize::NoRecvEvent, 則會關閉 OnDevice_Recv() 事件.
   NoRecvEvent = 0x02,
};
fon9_ENABLE_ENUM_BITWISE_OP(DeviceCommonOption);

fon9_MSC_WARN_DISABLE(4251);//dll-interface.
/// \ingroup io
/// 通訊設備基底.
class fon9_API Device : public intrusive_ref_counter<Device> {
   fon9_NON_COPY_NON_MOVE(Device);

   State                State_;
   Bookmark             SessionBookmark_{0};
   Bookmark             ManagerBookmark_{0};
   std::string          DeviceId_;
   DeviceCommonOption   CommonOptions_{DeviceCommonOption::SendASAP};

public:
   const Style       Style_;
   const SessionSP   Session_;
   const ManagerSP   Manager_;

   /// - 建構者必須在建構完畢, 取得一個 DeviceSP 之後, 立即呼叫 Initialize();
   /// - 必須提供有效的 ses, 運行過程中不會再檢查 this->Session_ 是否有效.
   /// - mgr 可以為 nullptr.
   Device(Style style, SessionSP ses, ManagerSP mgr)
      : State_{State::Initializing}
      , Style_{style}
      , Session_{std::move(ses)}
      , Manager_{std::move(mgr)} {
      assert(this->Session_);
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
   /// 到 OpThr_Dispose() 處理:
   /// - OpThr_Close(cause):
   ///    - 如果現在狀態 < State::Closing; 會先呼叫 this->OpImpl_Close(cause);
   /// - OpThr_SetState(State::Disposing, cause):
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
   /// \retval SysErr::not_connected  狀態不是 State::LinkReady 無法傳送.
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
      return IsEnumContains(this->CommonOptions_, DeviceCommonOption::SendASAP)
         ? this->SendASAP(src, size)
         : this->SendBuffered(src, size);
   }
   SendResult Send(BufferList&& src) {
      return IsEnumContains(this->CommonOptions_, DeviceCommonOption::SendASAP)
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
   std::string WaitSetProperty(StrView strTagValueList);

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

   /// 加入一個在 Device Op thread 執行的工作.
   void AddAsyncTask(DeviceAsyncTask task) {
      this->OpQueue_.AddTask(std::move(task));
   }

protected:
   friend struct DeviceAsyncOpInvoker;
   DeviceOpQueue  OpQueue_;

   /// 預設使用 GetDefaultThreadPool() 執行 OpQueue_.
   virtual void MakeCallForWork();

   // OpThr 或 OpImpl 開頭的函式, 都必須在 OpQueue_ thread 裡面呼叫:
   // - OpThr 通常為 OpQueue_.AddTask() 的進入點, 在檢查狀態後, 再決定如何呼叫 OpImpl.
   // - OpImpl 皆為 virtual, 交給衍生者實作.

   /// - 透過 OpThr_Open() 而來的呼叫, 必定: !cfgstr.IsNullOrEmpty(); && st<State::Disposing
   virtual void OpImpl_Open(std::string cfgstr) = 0;
   /// 透過 OpThr_Open() 而來的呼叫, 此時必定:
   /// - st < State::Disposing && st != State::OpenConfigError && st != Ready(LinkReady,Listening,WaitingLinkIn)
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

   State OpThr_GetState() const {
      return this->State_;
   }
   /// 現在狀態必須 < State::Disposing; 才允許呼叫 this->OpImpl_Open(cfgstr);
   void OpThr_Open(std::string cfgstr);
   /// 現在狀態必須 < State::Closing; 才需要呼叫 this->OpImpl_Close(cause);
   void OpThr_Close(std::string cause);
   void OpThr_LingerClose(std::string cause);
   void OpThr_Dispose(std::string cause);
   /// 每個 Device 在 LinkReady 時(有些在建構時,有些在Open成功時), 都有一個識別用的Id,
   /// - 例: TcpClient: "|R=RemoteIp:Port|L=LocalIp:Port"
   /// - 由於 WaitGetDeviceInfo() 會填入: "|id={DeivceId_}"; 所以 deviceId 不可有沒配對的大括號.
   void OpThr_SetDeviceId(std::string deviceId) {
      this->DeviceId_ = std::move(deviceId);
   }
   const std::string& OpThr_GetDeviceId() const {
      return this->DeviceId_;
   }
   /// \retval true  將 this->State_ 設為 afst,
   ///               並觸發 Session::OnDevice_StateChanged(), Manager::OnDevice_StateChanged() 事件.
   /// \retval false 狀態不須變動(this->State_==afst): 觸發 OnDevice_StateUpdated();
   ///               或無法變動(已進入 Disposing 程序), 不會觸發任何事件.
   bool OpThr_SetState(State afst, StrView stmsg);

   /// OpThr_SetLinkReady() 的流程:
   /// - 衍生者: 發現 LinkReady, 進入 Op thread 呼叫 OpThr_SetLinkReady();
   /// - 在 OpThr_SetLinkReady() 裡面:
   ///   - 衍生者如果要「設定傳送緩衝的LinkReady狀態」應在 OpImpl_StateChanged() 裡面:
   ///      \code
   ///         virtual void OpImpl_StateChanged(const StateChangedArgs& e) override {
   ///            if (e.After_ == State::LinkReady)
   ///               this->SendBuffer_.SetLinkReady();
   ///            base::OpImpl_StateChanged(e);
   ///         }
   ///      \endcode
   ///   - this->OpThr_SetState(State::LinkReady, stmsg);
   ///   - if (this->State_ == State::LinkReady)
   ///      this->OpThr_StartRecv(this->Session_->OnDevice_LinkReady(*this));
   void OpThr_SetLinkReady(std::string stmsg);
   /// preallocSize = this->Session_->OnDevice_LinkReady(*this);
   virtual void OpThr_StartRecv(RecvBufferSize preallocSize);

   void OpThr_SetBrokenState(std::string cause);
   void AsyncSetBrokenState(std::string cause) {
      this->OpQueue_.AddTask(DeviceAsyncOp(&Device::OpThr_SetBrokenState, std::move(cause)));
   }

   /// \copydoc DeviceCommand()
   virtual std::string OnDevice_Command(StrView cmd, StrView param);

   /// - 解析 propList(tag=value) 然後呼叫 OpImpl_SetProperty(tag,value);
   /// - 一旦發生錯誤, 就不會繼續解析之後的內容.
   /// - 傳回 retval.empty() 表示成功.
   std::string OpImpl_SetPropertyList(StrView propList);
   virtual std::string OpImpl_SetProperty(StrView tag, StrView value);

   void SetNoRecvEvent() {
      this->CommonOptions_ |= DeviceCommonOption::NoRecvEvent;
   }
};
fon9_MSC_WARN_POP;

inline void DeviceAsyncOpInvoker::MakeCallForWork() {
   Device&  owner = ContainerOf(DeviceOpQueue::StaticCast(*this), &Device::OpQueue_);
   owner.MakeCallForWork();
}
inline void DeviceAsyncOpInvoker::Invoke(DeviceAsyncOp& task) {
   Device& dev = ContainerOf(DeviceOpQueue::StaticCast(*this), &Device::OpQueue_);
   if (task.FnAsync_)
      (dev.*task.FnAsync_)(std::move(task.FnAsyncArg_));
   else
      task.AsyncTask_(dev);
}

} } // namespaces
#endif//__fon9_io_Device_hpp__
