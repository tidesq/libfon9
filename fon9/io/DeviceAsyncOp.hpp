/// \file fon9/io/DeviceAsyncOp.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_DeviceAsyncOp_hpp__
#define __fon9_io_DeviceAsyncOp_hpp__
#include "fon9/io/IoBase.hpp"
#include "fon9/AQueue.hpp"

namespace fon9 { namespace io {

typedef void (*FnDeviceAsyncOp)(Device&, std::string);
using DeviceAsyncTask = std::function<void(Device&)>;

fon9_MSC_WARN_DISABLE(4371 // '' layout of class may have changed from a previous version of the compiler due to better packing of membe ''
                      4582 4583 /*4582/4583: constructor/destructor is not implicitly called*/);
struct DeviceAsyncOp {
   fon9_NON_COPYABLE(DeviceAsyncOp);
   FnDeviceAsyncOp      FnAsync_;
   union {
      std::string       FnAsyncArg_;
      DeviceAsyncTask   AsyncTask_;
   };
   DeviceAsyncOp(FnDeviceAsyncOp fnAsync, std::string arg) : FnAsync_{fnAsync}, FnAsyncArg_{std::move(arg)} {
   }
   DeviceAsyncOp(DeviceAsyncTask task) : FnAsync_{nullptr}, AsyncTask_{std::move(task)} {
   }

   DeviceAsyncOp(DeviceAsyncOp&& rhs) {
      this->InplaceMove(std::move(rhs));
   }
   DeviceAsyncOp& operator=(DeviceAsyncOp&& rhs) {
      this->Destroy();
      this->InplaceMove(std::move(rhs));
      return *this;
   }

   ~DeviceAsyncOp() {
      this->Destroy();
   }
private:
   void InplaceMove(DeviceAsyncOp&& rhs) {
      if ((this->FnAsync_ = rhs.FnAsync_) != nullptr)
         InplaceNew<std::string>(&this->FnAsyncArg_, std::move(rhs.FnAsyncArg_));
      else
         InplaceNew<DeviceAsyncTask>(&this->AsyncTask_, std::move(rhs.AsyncTask_));
   }

   void Destroy() {
      if (this->FnAsync_)
         (&this->FnAsyncArg_)->std::string::~string();
      else
         (&this->AsyncTask_)->DeviceAsyncTask::~DeviceAsyncTask();
   }
};
fon9_MSC_WARN_POP;

struct DeviceAsyncOpInvoker {
   void Invoke(DeviceAsyncOp& task);
   void MakeCallForWork();
   
   template <class Locker>
   void MakeCallForWork(Locker& locker) {
      locker.unlock();
      this->MakeCallForWork();
   }

   /// Waiter = CountDownLatch;
   template <class Waiter>
   DeviceAsyncOp MakeWaiterTask(DeviceAsyncOp& task, Waiter& waiter) {
      return DeviceAsyncOp{[&](Device&) {
         this->Invoke(task);
         waiter.ForceWakeUp();
      }};
   }
};

using DeviceOpQueue = AQueue<DeviceAsyncOp, DeviceAsyncOpInvoker>;

struct DeviceOpLocker {
   using ALocker = DeviceOpQueue::ALockerForInplace;

   void Create(Device& dev, AQueueTaskKind taskKind);

   void Destroy() {
      this->ALocker_.clear();
   }

   Device& GetDevice() const;

   ALocker& GetALocker() {
      return *this->ALocker_;
   }

protected:
   DyObj<ALocker> ALocker_;
};

} } // namespaces
#endif//__fon9_io_DeviceAsyncOp_hpp__
