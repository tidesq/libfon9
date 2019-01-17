/// \file fon9/io/FdrSocket.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_FdrSocket_hpp__
#define __fon9_io_FdrSocket_hpp__
#include "fon9/io/FdrService.hpp"
#include "fon9/io/DeviceStartSend.hpp"
#include "fon9/io/DeviceRecvEvent.hpp"
#include "fon9/io/Socket.hpp"

namespace fon9 { namespace io {

class FdrSocket : public FdrEventHandler {
protected:
   using FdrEventFlagU = std::underlying_type<FdrEventFlag>::type;
   std::atomic<FdrEventFlagU> EnabledEvents_{0};
   RecvBufferSize             RecvSize_;
   RecvBuffer                 RecvBuffer_;
   SendBuffer                 SendBuffer_;

   /// 建立錯誤訊息字串, 觸發事件:
   /// `this->OnFdrSocket_Error("fnName:" + GetSocketErrC(eno));`
   virtual void SocketError(StrView fnName, int eno);
   virtual void OnFdrSocket_Error(std::string errmsg) = 0;

   virtual FdrEventFlag GetRequiredFdrEventFlag() const override;

   void CheckSocketErrorOrCanceled(FdrEventFlag evs) {
      if (IsEnumContains(evs, FdrEventFlag::Error)) {
         this->SocketError("Event", Socket::LoadSocketErrno(this->GetFD()));
      }
      else if (IsEnumContains(evs, FdrEventFlag::OperationCanceled)) {
         this->SocketError("Cancel", ECANCELED);
      }
   }

   /// \retval 0     success;  返回前, 若已無資料則: CheckSendQueueEmpty(); 若仍有資料則: 啟動 writable 偵測.
   /// \retval else  errno;    返回前, 已先呼叫 this->OnFdrSocket_Error("fn=Sendv|err=", retval);
   int Sendv(DeviceOpLocker& sc, DcQueueList& toSend);
   
   bool SetDisableEventBit(FdrEventFlag ev) {
      return (this->EnabledEvents_.fetch_and(~static_cast<FdrEventFlagU>(ev), std::memory_order_relaxed)
              & static_cast<FdrEventFlagU>(ev)) != 0;
   }

public:
   FdrSocket(FdrService& iosv, Socket&& so) : FdrEventHandler{iosv, so.MoveOut()} {
   }

   void EnableEventBit(FdrEventFlag ev) {
      if ((this->EnabledEvents_.fetch_or(static_cast<FdrEventFlagU>(ev), std::memory_order_relaxed)
           & static_cast<FdrEventFlagU>(ev)) == 0)
         this->UpdateFdrEvent();
   }
   void DisableEventBit(FdrEventFlag ev) {
      if (this->SetDisableEventBit(ev))
         this->UpdateFdrEvent();
   }
   void DisableEvent() {
      if (this->EnabledEvents_.fetch_and(0, std::memory_order_relaxed) != 0)
         this->UpdateFdrEvent();
   }

   //--------------------------------------------------------------------------//

   struct FdrEventAux {
      FdrSocket*  IsNeedsUpdateFdrEvent_;
      FdrEventAux() : IsNeedsUpdateFdrEvent_{nullptr} {
      }
      ~FdrEventAux() {
         if (this->IsNeedsUpdateFdrEvent_)
            this->IsNeedsUpdateFdrEvent_->UpdateFdrEvent();
      }
      void DisableEvent(FdrSocket& impl, FdrEventFlag ev) {
         // 現在還是 alocker 的狀態! 所以只設定 Flag_, this 解構時再呼叫 UpdateFdrEvent();
         if (impl.SetDisableEventBit(ev))
            this->IsNeedsUpdateFdrEvent_ = &impl;
      }
   };
      
   void StartRecv(RecvBufferSize expectSize) {
      this->RecvSize_ = expectSize;
      this->EnableEventBit(FdrEventFlag::Readable);
   }
   void ContinueRecv(RecvBufferSize expectSize, bool isEnableReadable) {
      this->RecvSize_ = expectSize;
      if (isEnableReadable)
         this->EnableEventBit(FdrEventFlag::Readable);
   }
   RecvBuffer& GetRecvBuffer() {
      return this->RecvBuffer_;
   }
   struct FdrRecvAux : public FdrEventAux {
      static void ContinueRecv(RecvBuffer& rbuf, RecvBufferSize expectSize, bool isEnableReadable) {
         ContainerOf(rbuf, &FdrSocket::RecvBuffer_).ContinueRecv(expectSize, isEnableReadable);
      }
      void DisableReadableEvent(RecvBuffer& rbuf) {
         this->DisableEvent(ContainerOf(rbuf, &FdrSocket::RecvBuffer_), FdrEventFlag::Readable);
      }
      static SendDirectResult SendDirect(RecvDirectArgs& e, BufferList&& txbuf);
   };

   /// \retval true  成功完成 read.
   /// \retval false read 失敗, 返回前已呼叫 OnFdrSocket_Error();
   bool CheckRead(Device& dev, bool (*fnIsRecvBufferAlive)(Device& dev, RecvBuffer& rbuf));

   //--------------------------------------------------------------------------//

   SendBuffer& GetSendBuffer() {
      return this->SendBuffer_;
   }
   void CheckSendQueueEmpty(DeviceOpLocker& sc);

   struct ContinueSendAux : public FdrEventAux {
      void DisableWritableEvent(SendBuffer& sbuf) {
         this->DisableEvent(ContainerOf(sbuf, &FdrSocket::SendBuffer_), FdrEventFlag::Writable);
      }
      static DcQueueList* GetContinueToSend(SendBuffer& sbuf) {
         return sbuf.OpImpl_CheckSendQueue();
      }
      void ContinueToSend(ContinueSendChecker& sc, DcQueueList& toSend) const {
         sc.GetALocker().UnlockForInplace();
         // 可能的呼叫點:
         // - 在 io thread 的 writable 事件裡面呼叫.
         // - DisableWritableEvent() 之後, 在 op thread 的 Async.
         SendBuffer& sbuf = SendBuffer::StaticCast(toSend);
         FdrSocket&  impl = ContainerOf(sbuf, &FdrSocket::SendBuffer_);
         impl.Sendv(sc, toSend);
      }
   };

   struct SendASAP_AuxMem : public SendAuxMem {
      using SendAuxMem::SendAuxMem;

      Device::SendResult StartToSend(DeviceOpLocker& sc, DcQueueList& toSend) {
         FdrSocket&  impl = ContainerOf(SendBuffer::StaticCast(toSend), &FdrSocket::SendBuffer_);
         auto        wrsz = write(impl.GetFD(), this->Src_, this->Size_);
         if (fon9_UNLIKELY(wrsz < 0)) {
            if (int eno = ErrorCannotRetry(errno)) {
               impl.SocketError("Send", eno);
               return GetSysErrC(eno);
            }
            wrsz = 0;
         }

         if (fon9_LIKELY(static_cast<size_t>(wrsz) >= this->Size_))
            impl.CheckSendQueueEmpty(sc);
         else {
            toSend.Append(reinterpret_cast<const char*>(this->Src_) + wrsz, this->Size_ - wrsz);
            impl.EnableEventBit(FdrEventFlag::Writable);
         }
         return Device::SendResult{static_cast<size_t>(wrsz)};
      }
   };

   struct SendASAP_AuxBuf : public SendAuxBuf {
      using SendAuxBuf::SendAuxBuf;

      Device::SendResult StartToSend(DeviceOpLocker& sc, DcQueueList& toSend) {
         FdrSocket&  impl = ContainerOf(SendBuffer::StaticCast(toSend), &FdrSocket::SendBuffer_);
         toSend.push_back(std::move(*this->Src_));
         if (int eno = impl.Sendv(sc, toSend))
            return GetSysErrC(eno);
         return Device::SendResult{0};
      }
   };

   struct SendBuffered_AuxMem : public SendAuxMem {
      using SendAuxMem::SendAuxMem;

      Device::SendResult StartToSend(DeviceOpLocker&, DcQueueList& toSend) {
         bool doNeedsNotify = toSend.empty();
         toSend.Append(this->Src_, this->Size_);
         if (doNeedsNotify) {
            FdrSocket&  impl = ContainerOf(SendBuffer::StaticCast(toSend), &FdrSocket::SendBuffer_);
            impl.StartSendInFdrThread();
         }
         return Device::SendResult{0};
      }
   };

   struct SendBuffered_AuxBuf : public SendAuxBuf {
      using SendAuxBuf::SendAuxBuf;

      Device::SendResult StartToSend(DeviceOpLocker&, DcQueueList& toSend) {
         bool doNeedsNotify = toSend.empty();
         toSend.push_back(std::move(*this->Src_));
         if (doNeedsNotify) {
            FdrSocket&  impl = ContainerOf(SendBuffer::StaticCast(toSend), &FdrSocket::SendBuffer_);
            impl.StartSendInFdrThread();
         }
         return Device::SendResult{0};
      }
   };
};

} } // namespaces
#endif//__fon9_io_FdrSocket_hpp__
