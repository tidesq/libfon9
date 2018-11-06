/// \file fon9/io/FdrSocketClient.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_FdrSocketClient_hpp__
#define __fon9_io_FdrSocketClient_hpp__
#include "fon9/io/FdrSocket.hpp"

namespace fon9 { namespace io {

struct FdrSocketClientImpl : public FdrSocket, public intrusive_ref_counter<FdrSocketClientImpl> {
   fon9_NON_COPY_NON_MOVE(FdrSocketClientImpl);
   using baseCounter = intrusive_ref_counter<FdrSocketClientImpl>;
   virtual void OnFdrEvent_AddRef() override;
   virtual void OnFdrEvent_ReleaseRef() override;

protected:
   enum class State {
      Idle,
      Closing,
      Connecting,
      Connected,
   };
   State State_{};

   template <class OwnerDevice>
   void StartSend(OwnerDevice& owner) {
      typename OwnerDevice::ContinueSendAux aux;
      DeviceContinueSend(owner, this->SendBuffer_, aux);
   }

   template <class Impl, class OwnerDevice>
   static void FdrEventProcessor(Impl* impl, OwnerDevice& owner, FdrEventFlag evs) {
      if (fon9_UNLIKELY(impl->State_ == State::Closing))
         return;

      if (IsEnumContains(evs, FdrEventFlag::Readable)) {
         if (!impl->CheckRead(owner, &OwnerDevice::OpImpl_IsRecvBufferAlive))
            return;
      }

      if (IsEnumContains(evs, FdrEventFlag::Writable)) {
         if (fon9_LIKELY(impl->State_ == State::Connected)) {
            typename OwnerDevice::ContinueSendAux aux;
            DeviceContinueSend(owner, impl->SendBuffer_, aux);
         }
         else if (!IsEnumContains(evs, FdrEventFlag::Error)) {
            // 連線失敗用 FdrEventFlag::Error 方式通知.
            // 所以 !IsEnumContains(evs, FdrEventFlag::Error) 才需要處理連線成功事件 OnSocketConnected().
            impl->State_ = State::Connected;
            impl->DisableEvent();
            owner.OnSocketConnected(impl, impl->GetFD());
         }
      }

      impl->CheckSocketErrorOrCanceled(evs);
   }

public:
   FdrSocketClientImpl(FdrService& iosv, Socket&& so)
      : FdrSocket{iosv, std::move(so)} {
   }
   bool IsClosing() const {
      return this->State_ == State::Closing;
   }
   void OpImpl_Close();
};

} } // namespaces
#endif//__fon9_io_FdrSocketClient_hpp__
