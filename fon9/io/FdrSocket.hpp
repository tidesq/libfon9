/// \file fon9/io/FdrSocket.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_FdrSocket_hpp__
#define __fon9_io_FdrSocket_hpp__
#include "fon9/io/FdrService.hpp"
#include "fon9/io/IoBufferSend.hpp"
#include "fon9/io/Socket.hpp"

namespace fon9 { namespace io {

class FdrSocket : public IoBuffer, public FdrEventHandler {
protected:
   using FdrEventFlagU = std::underlying_type<FdrEventFlag>::type;
   std::atomic<FdrEventFlagU> EnabledEvents_{0};
   RecvBufferSize             RecvSize_;

   virtual FdrEventFlag GetRequiredFdrEventFlag() const override;

   virtual void OnFdrSocket_Error(StrView fnName, int eno) = 0;

   /// \retval 0     success;  返回前, 若已無資料則: CheckSendQueueEmpty(); 若仍有資料則: 啟動 writable 偵測.
   /// \retval else  errno;    返回前, 已先呼叫 this->OnFdrSocket_Error("Sendv", retval);
   int Sendv(Device& dev, DcQueueList& toSend);

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

   virtual void StartRecv(RecvBufferSize expectSize) override;

   struct FdrContinueSendAux {
      FdrSocket*  IsNeedsUpdateFdrEvent_;
      FdrContinueSendAux() : IsNeedsUpdateFdrEvent_{nullptr} {
      }
      ~FdrContinueSendAux() {
         if (this->IsNeedsUpdateFdrEvent_)
            this->IsNeedsUpdateFdrEvent_->UpdateFdrEvent();
      }
      void DisableWritableEvent(IoBuffer& iobuf) {
         // 現在還是 alocker 的狀態! 所以只設定 Flag_, 結束後再呼叫 Update();
         if (static_cast<FdrSocket*>(&iobuf)->SetDisableEventBit(FdrEventFlag::Writable))
            this->IsNeedsUpdateFdrEvent_ = static_cast<FdrSocket*>(&iobuf);
      }

      static DcQueueList* GetToSend(SendBuffer& sbuf) {
         return sbuf.OpImpl_CheckSendQueue();
      }
      void StartSend(Device& dev, IoBuffer& iobuf, DcQueueList& toSend) const {
         static_cast<FdrSocket*>(&iobuf)->Sendv(dev, toSend);
      }
   };

   void CheckSendQueueEmpty(Device& dev);

   struct SendASAP_AuxMem : public SendAuxMem {
      using SendAuxMem::SendAuxMem;
      using ImplProtector = FdrEventHandlerSP;

      Device::SendResult Send(Device& dev, FdrSocket& impl, DcQueueList& toSend) {
         ssize_t wrsz = write(impl.GetFD(), this->Src_, this->Size_);
         if (fon9_UNLIKELY(wrsz < 0)) {
            int eno = errno;
            impl.OnFdrSocket_Error("Send", eno);
            return GetSysErrC(eno);
         }

         if (fon9_LIKELY(static_cast<size_t>(wrsz) >= this->Size_))
            impl.CheckSendQueueEmpty(dev);
         else {
            toSend.Append(reinterpret_cast<const char*>(this->Src_) + wrsz, this->Size_ - wrsz);
            impl.EnableEventBit(FdrEventFlag::Writable);
         }
         return Device::SendResult{static_cast<size_t>(wrsz)};
      }
   };

   struct SendASAP_AuxBuf : public SendAuxBuf {
      using SendAuxBuf::SendAuxBuf;
      using ImplProtector = FdrEventHandlerSP;

      Device::SendResult Send(Device& dev, FdrSocket& impl, DcQueueList& toSend) {
         toSend.push_back(std::move(*this->Src_));
         if (int eno = impl.Sendv(dev, toSend))
            return GetSysErrC(eno);
         return Device::SendResult{0};
      }
   };

   struct SendBuffered_AuxMem : public SendAuxMem {
      using SendAuxMem::SendAuxMem;
      using ImplProtector = FdrEventHandlerSP;

      Device::SendResult Send(Device&, FdrSocket& impl, DcQueueList& toSend) {
         toSend.Append(this->Src_, this->Size_);
         impl.StartSendInFdrThread();
         return Device::SendResult{0};
      }
   };

   struct SendBuffered_AuxBuf : public SendAuxBuf {
      using SendAuxBuf::SendAuxBuf;
      using ImplProtector = FdrEventHandlerSP;

      Device::SendResult Send(Device&, FdrSocket& impl, DcQueueList& toSend) {
         toSend.push_back(std::move(*this->Src_));
         impl.StartSendInFdrThread();
         return Device::SendResult{0};
      }
   };
};

} } // namespaces
#endif//__fon9_io_FdrSocket_hpp__
