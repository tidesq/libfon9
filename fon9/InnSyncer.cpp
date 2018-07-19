/// \file fon9/InnSyncer.cpp
/// \author fonwinz@gmail.com
#include "fon9/InnSyncer.hpp"
#include "fon9/BitvEncode.hpp"
#include "fon9/BitvDecode.hpp"
#include "fon9/Log.hpp"

namespace fon9 {

InnSyncHandler::~InnSyncHandler() {
}

//--------------------------------------------------------------------------//

InnSyncer::~InnSyncer() {
}

bool InnSyncer::DetachHandler(InnSyncHandler& handler) {
   HandlerMap::Locker handlers{this->HandlerMap_};
   auto   ifind = handlers->find(ToStrView(handler.SyncHandlerName_));
   if (ifind == handlers->end() || ifind->second != &handler)
      return false;
   handlers->erase(ifind);
   return true;
}
bool InnSyncer::AttachHandler(InnSyncHandlerSP handler) {
   HandlerMap::Locker handlers{this->HandlerMap_};
   return handlers->insert(HandlerMapImpl::value_type{ToStrView(handler->SyncHandlerName_), handler}).second;
}

void InnSyncer::WriteSync(InnSyncHandler& handler, RevBufferList&& rbuf) {
   ToBitv(rbuf, handler.SyncHandlerName_);
   ByteArraySizeToBitvT(rbuf, CalcDataSize(rbuf.cfront()));
   this->WriteSyncImpl(std::move(rbuf));
}

size_t InnSyncer::OnInnSyncRecv(DcQueue& buf) {
   size_t pkcount = 0;
   for (;;) {
      size_t pksz;
      if (!PopBitvByteArraySize(buf, pksz))
         return pkcount;
      ++pkcount;
      CharVector  handlerName;
      BitvTo(buf, handlerName);

      auto curblk = buf.PeekCurrBlock();
      if (curblk.second >= pksz) {
         this->OnInnSyncRecvImpl(ToStrView(handlerName), DcQueueFixedMem{curblk.first, pksz});
         buf.PopConsumed(pksz);
      }
      else {
         ByteVector pkbuf;
         buf.Read(pkbuf.alloc(pksz), pksz);
         this->OnInnSyncRecvImpl(ToStrView(handlerName), DcQueueFixedMem{pkbuf.begin(), pksz});
      }
   }
}
void InnSyncer::OnInnSyncRecvImpl(StrView handlerName, DcQueue&& buf) {
   InnSyncHandlerSP   handler;
   {
      HandlerMap::Locker handlers{this->HandlerMap_};
      auto ifind = handlers->find(handlerName);
      if (ifind == handlers->end()) {
         // 找不到要同步的 handler: 此 handler 沒註冊? 或已不須此資料?
         fon9_LOG_WARN("InnSyncer.OnInnSyncRecv|handlerName=", handlerName, "|err=handler not found.");
         return;
      }
      handler = ifind->second;
   } // unlock
   handler->OnInnSyncReceived(*this, std::move(buf));
}

} // namespaces
