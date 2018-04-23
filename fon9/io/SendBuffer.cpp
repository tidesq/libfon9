// \file fon9/io/SendBuffer.cpp
// \author fonwinz@gmail.com
#include "fon9/io/SendBuffer.hpp"

namespace fon9 { namespace io {

void SendBuffer::ClearBuffer(const ErrC& errc, BufferStatus st) {
   Locker buf{this->Buffer_};
   buf->Status_ = st;
   DcQueueList tmpbuf{std::move(buf->SendingBuffer_)};
   tmpbuf.push_back(std::move(buf->QueuingBuffer_));
   buf.unlock();
   tmpbuf.ConsumeErr(errc);
}
DcQueueList* SendBuffer::LockForConsume(Locker& buf) {
   if (buf->Status_ == BufferStatus::Sending)
      return nullptr;
   buf->SendingBuffer_.push_back(std::move(buf->QueuingBuffer_));
   return (buf->SendingBuffer_.empty() ? nullptr : &buf->SendingBuffer_);
}
DcQueueList* SendBuffer::LockForConsume() {
   Locker buf{this->Buffer_};
   if (DcQueueList* retval = this->LockForConsume(buf)) {
      buf->Status_ = BufferStatus::Sending;
      return retval;
   }
   return nullptr;
}
SendBuffer::LockResult SendBuffer::LockForASAP(Locker& buf) {
   if (buf->Status_ < BufferStatus::LinkReady)
      return LockResult::NotReady;
   if (buf->IsQueuingRequired())
      return LockResult::Queuing;
   buf->Status_ = BufferStatus::Sending;
   return LockResult::AllowASAP;
}
SendBuffer::LockResult SendBuffer::LockForASAP(const void* src, size_t size) {
   Locker      buf{this->Buffer_};
   LockResult  res = this->LockForASAP(buf);
   if (res == LockResult::Queuing)
      AppendToBuffer(buf->QueuingBuffer_, src, size);
   return res;
}
SendBuffer::LockResult SendBuffer::LockForASAP(BufferList&& src, DcQueueList*& outbuf) {
   Locker      buf{this->Buffer_};
   LockResult  res = this->LockForASAP(buf);
   switch (res) {
   case LockResult::AllowASAP:
      outbuf = &buf->SendingBuffer_;
      outbuf->push_back(std::move(src));
      break;
   case LockResult::Queuing:
      buf->QueuingBuffer_.push_back(std::move(src));
      break;
   default:
   case LockResult::NotReady:
      BufferListConsumeErr(std::move(src), std::errc::no_link);
      break;
   }
   return res;
}
SendBuffer::PushResult SendBuffer::PushSend(Locker& buf, BufferList&& src) {
   if (buf->Status_ < BufferStatus::LinkReady) {
      BufferListConsumeErr(std::move(src), std::errc::no_link);
      return PushResult::NotReady;
   }
   if (buf->IsQueuingRequired()) {
      buf->QueuingBuffer_.push_back(std::move(src));
      return PushResult::Queuing;
   }
   buf->SendingBuffer_.push_back(std::move(src));
   return PushResult::New;
}
SendBuffer::PushResult SendBuffer::PushSend(Locker& buf, const void* src, size_t size) {
   if (buf->Status_ < BufferStatus::LinkReady)
      return PushResult::NotReady;
   if (buf->IsQueuingRequired()) {
      AppendToBuffer(buf->QueuingBuffer_, src, size);
      return PushResult::Queuing;
   }
   BufferList   tmpbuf;
   AppendToBuffer(tmpbuf, src, size);
   buf->SendingBuffer_.push_back(std::move(tmpbuf));
   return PushResult::New;
}
SendBuffer::AfterSent SendBuffer::AfterConsumed(DcQueueList* outbuf) {
   SendBuffer::Locker buf{this->Buffer_};
   assert(buf->Status_ == BufferStatus::Sending && outbuf == &buf->SendingBuffer_);
   buf->Status_ = BufferStatus::LinkReady;
   bool isNoRemain = outbuf->empty();
   outbuf->push_back(std::move(buf->QueuingBuffer_));
   if (isNoRemain)
      return outbuf->empty() ? AfterSent::BufferEmpty : AfterSent::NewArrive;
   return AfterSent::HasRemain;
}
SendBuffer::AfterSent SendBuffer::AfterASAP(const void* srcRemain, size_t sizeRemain) {
   SendBuffer::Locker buf{this->Buffer_};
   assert(buf->Status_ == BufferStatus::Sending && buf->IsEmpty());
   buf->Status_ = BufferStatus::LinkReady;
   if (srcRemain && sizeRemain) {
      BufferList   tmpbuf;
      AppendToBuffer(tmpbuf, srcRemain, sizeRemain);
      buf->SendingBuffer_.push_back(std::move(tmpbuf));
      buf->SendingBuffer_.push_back(std::move(buf->QueuingBuffer_));
      return AfterSent::HasRemain;
   }
   buf->SendingBuffer_.push_back(std::move(buf->QueuingBuffer_));
   return buf->SendingBuffer_.empty() ? AfterSent::BufferEmpty : AfterSent::NewArrive;
}

} } // namespaces
