/// \file fon9/io/DeviceAsyncOp.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_DeviceAsyncOp_hpp__
#define __fon9_io_DeviceAsyncOp_hpp__
#include "fon9/io/IoBase.hpp"
#include "fon9/AQueue.hpp"

namespace fon9 { namespace io {

typedef void (Device::*FnDeviceAsyncOp)(std::string);
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

   DeviceAsyncOp(DeviceAsyncOp&& rhs) : FnAsync_{rhs.FnAsync_} {
      if (rhs.FnAsync_)
         this->FnAsyncArg_ = std::move(rhs.FnAsyncArg_);
      else
         this->AsyncTask_ = std::move(rhs.AsyncTask_);
   }
   DeviceAsyncOp& operator=(DeviceAsyncOp&& rhs) {
      this->Destroy();
      if ((this->FnAsync_ = rhs.FnAsync_) != nullptr)
         InplaceNew<std::string>(&this->FnAsyncArg_, std::move(rhs.FnAsyncArg_));
      else
         InplaceNew<DeviceAsyncTask>(&this->AsyncTask_, std::move(rhs.AsyncTask_));
      return *this;
   }

   ~DeviceAsyncOp() {
      this->Destroy();
   }
private:
   void Destroy() {
      if (this->FnAsync_)
         (&this->FnAsyncArg_)->std::string::~string();
      else
         (&this->AsyncTask_)->DeviceAsyncTask::~DeviceAsyncTask();
   }
};
fon9_MSC_WARN_POP;

struct DeviceAsyncOpInvoker {
   void MakeCallForWork();
   void Invoke(DeviceAsyncOp& task);

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

} } // namespaces
#endif//__fon9_io_DeviceAsyncOp_hpp__
