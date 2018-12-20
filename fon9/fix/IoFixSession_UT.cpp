// \file fon9/fix/IoFixSession_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "fon9/TestTools.hpp"
#include "fon9/fix/IoFixSession.hpp"
#include "fon9/fix/IoFixSender.hpp"
#include "fon9/fix/FixAdminMsg.hpp"
#include "fon9/buffer/FwdBufferList.hpp"
#include "fon9/DefaultThreadPool.hpp"

namespace f9fix = fon9::fix;

//--------------------------------------------------------------------------//
fon9::StrView fgetstr(char* strbuf, int bufsz, FILE* fin) {
   for (;;) {
      if (!fgets(strbuf, bufsz, fin))
         return fon9::StrView{};
      fon9::StrView  str{fon9::StrView_cstr(strbuf)};
      if (fon9::StrTrim(&str) == "q")
         return nullptr;
      return str;
   }
}
//--------------------------------------------------------------------------//
static const uint32_t   kHeartBtInt = 5;
static const char       kFixTestInitiatorRecorderFileName[] = "./FixTestI.log";
static const char       kFixTestAcceptorRecorderFileName[] = "./FixTestA.log";
enum class ConnectionType : char {
   Initiator = 'I',
   Acceptor = 'A'
};

fon9_WARN_DISABLE_PADDING;
struct FixTestMgr : public f9fix::IoFixManager {
   fon9_NON_COPY_NON_MOVE(FixTestMgr);
   using base = f9fix::IoFixManager;
   f9fix::IoFixSenderSP FixOut_;
   ConnectionType       ConnectionType_;

   FixTestMgr(ConnectionType connectionType) : ConnectionType_{connectionType} {
      switch (connectionType) {
      case ConnectionType::Acceptor:
         this->FixOut_.reset(new f9fix::IoFixSender{f9fix_BEGIN_HEADER_V42, f9fix::CompIDs{"AComp", "ASub", "IComp", "ISub"}});
         this->FixOut_->GetFixRecorder().Initialize(kFixTestAcceptorRecorderFileName);
         break;
      case ConnectionType::Initiator:
         this->FixOut_.reset(new f9fix::IoFixSender{f9fix_BEGIN_HEADER_V42, f9fix::CompIDs{"IComp", "ISub", "AComp", "ASub"}});
         this->FixOut_->GetFixRecorder().Initialize(kFixTestInitiatorRecorderFileName);
         break;
      }
   }

   void OnFixSessionDisconnected(f9fix::IoFixSession&, f9fix::FixSenderSP&& fixout) override {
      if (f9fix::IoFixSender* devout = dynamic_cast<f9fix::IoFixSender*>(fixout.get()))
         devout->OnFixSessionDisconnected();
   }
   void OnFixSessionConnected(f9fix::IoFixSession& fixses) override {
      if (this->ConnectionType_ == ConnectionType::Initiator) {
         f9fix::FixBuilder  fixb;
         fon9::RevPrint(fixb.GetBuffer(), f9fix_kFLD_EncryptMethod_None);
         this->FixOut_->OnFixSessionConnected(fixses.GetDevice());
         this->OnLogonInitiate(fixses, kHeartBtInt, std::move(fixb), this->FixOut_);
      }
   }
   void OnRecvLogonRequest(f9fix::FixRecvEvArgs& rxargs) override {
      if (this->ConnectionType_ != ConnectionType::Acceptor) {
         // 不是 Acceptor 卻來到這兒, 必定是設計有誤!
         std::cout << "\n[ERROR] OnRecvLogon|err=FixSession is not Acceptor!" << std::endl;
         abort();
      }
      // 根據 CompID & 「rxarg.FixSession_ 的設定」 取得 FixSender.
      // 這裡只是單元測試, 所以直接使用 this->FixOut_;
      if (this->FixOut_->GetFixRecorder().CompIDs_.Check(rxargs.Msg_) != 0) {
         // 正式環境會根據 LogonRequest 的 CompIDs 來取得對應的 FixSender.
         rxargs.FixSession_->SendLogout(fon9::StrView{"Bad Logon, CompID problem."});
         return;
      }
      this->FixOut_->OnFixSessionConnected(static_cast<f9fix::IoFixSession*>(rxargs.FixSession_)->GetDevice());
      if (!this->OnLogonAccepted(rxargs, this->FixOut_))
         this->FixOut_->OnFixSessionDisconnected();
   }
   virtual void OnFixSessionApReady(f9fix::IoFixSession& fixses) override {
      fon9_LOG_DEBUG("ApReady|from=", fixses.GetDeviceId());
   }
};
fon9_WARN_POP;
//--------------------------------------------------------------------------//
class TestDevice : public fon9::io::Device {
   fon9_NON_COPY_NON_MOVE(TestDevice);
   using base = fon9::io::Device;
   fon9::DcQueueList RxBuffer_;
   void SetLinkReady() {
      fon9::StrView devid{"R=TestDevice"};
      fon9::io::Device::OpThr_SetDeviceId(*this, devid.ToString());
      fon9::io::Device::OpThr_SetLinkReady(*this, devid.ToString());
   }
   void OpImpl_Open(std::string cfgstr) override {
      fon9_LOG_DEBUG("Device.Open|cfg=", cfgstr);
      this->SetLinkReady();
   }
   void OpImpl_Reopen() override {
      fon9_LOG_DEBUG("Device.Reopen");
      this->SetLinkReady();
   }
   void OpImpl_Close(std::string cause) override {
      fon9_LOG_DEBUG("Device.Close|cause=", cause);
      this->OpImpl_SetState(fon9::io::State::Closed, &cause);
   }
public:
   TestDevice(f9fix::IoFixSessionSP fixses)
      : base{std::move(fixses), nullptr, fon9::io::Style::Simulation} {
   }
   bool IsSendBufferEmpty() const override {
      return true;
   }
   SendResult SendASAP(const void* src, size_t size) override {
      fon9_LOG_DEBUG("Send:", fon9::StrView{reinterpret_cast<const char*>(src), size});
      return SendResult{};
   }
   SendResult SendASAP(fon9::BufferList&& src) override {
      fon9_LOG_DEBUG("Send:", src);
      size_t srcsz = CalcDataSize(src.cfront());
      fon9::DcQueueList{std::move(src)}.PopConsumed(srcsz);
      return SendResult{};
   }
   SendResult SendBuffered(const void* src, size_t size) override {
      return this->SendASAP(src, size);
   }
   SendResult SendBuffered(fon9::BufferList&& src) override {
      return this->SendASAP(std::move(src));
   }
   void OnReceive(const fon9::StrView& str) {
      this->OpQueue_.InplaceOrWait(fon9::AQueueTaskKind::Recv,
                                   fon9::io::DeviceAsyncOp([](Device& dev, std::string rxstr) {
         static_cast<TestDevice*>(&dev)->RxBuffer_.Append(rxstr.c_str(), rxstr.size());
         if (dev.Session_->OnDevice_Recv(dev, static_cast<TestDevice*>(&dev)->RxBuffer_) == fon9::io::RecvBufferSize::NoRecvEvent)
            fon9_LOG_DEBUG("OnReceive.NoRecvEvent");
      }, str.ToString()));
   }
};
using TestDeviceSP = fon9::intrusive_ptr<TestDevice>;
//--------------------------------------------------------------------------//
struct FixSessionTester {
   fon9_NON_COPY_NON_MOVE(FixSessionTester);
   FixTestMgr              FixMgr_;
   f9fix::FixConfig        FixConfig_;
   f9fix::IoFixSessionSP   FixSes_;
   TestDeviceSP            Dev_;
   FixSessionTester(ConnectionType connectionType) : FixMgr_{connectionType} {
      f9fix::FixSession::InitFixConfig(this->FixConfig_);
      this->FixSes_.reset(new f9fix::IoFixSession{this->FixMgr_, this->FixConfig_});
      this->Dev_.reset(new TestDevice{this->FixSes_});
      this->Dev_->Initialize();
      this->Dev_->AsyncOpen("devopen");
   }
   ~FixSessionTester() {
      this->Dev_->AsyncDispose("quit");
      this->FixSes_.reset();
      this->Dev_->WaitGetDeviceId(); // 等候 AsyncDispose() 結束.
   }
};
//--------------------------------------------------------------------------//
void TestCommandFixSession(ConnectionType connectionType) {
   FixSessionTester tester{connectionType};
   char             strbuf[f9fix::FixRecorder::kMaxFixMsgBufferSize];
   for (;;) {
      printf("%c> ", static_cast<char>(connectionType));
      fon9::StrView  fixmsg{fgetstr(strbuf, sizeof(strbuf), stdin)};
      if (fixmsg.begin() == nullptr)
         break;
      if (fixmsg.empty())
         continue;
      switch (fixmsg.Get1st()) {
      case '#':
         continue;
      case 's': // sleep TimeInterval;
         fixmsg.SetBegin(fixmsg.begin() + 1);
         fon9::TimeInterval ti = fon9::StrTo(fixmsg, fon9::TimeInterval{});
         std::this_thread::sleep_for(ti.ToDuration());
         continue;
      }
      tester.Dev_->OnReceive(fixmsg);
   }
}
//--------------------------------------------------------------------------//
int main(int argc, char** args) {
   (void)argc; (void)args;
   #if defined(_MSC_VER) && defined(_DEBUG)
      _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
      //_CrtSetBreakAlloc(176);
   #endif
   fon9::AutoPrintTestInfo utinfo{"FixSender"};
   fon9::GetDefaultTimerThread();
   fon9::GetDefaultThreadPool();
   std::this_thread::sleep_for(std::chrono::milliseconds{10});

   std::remove(kFixTestInitiatorRecorderFileName);
   std::remove(kFixTestAcceptorRecorderFileName);
   for (;;) {
      printf("Connection type(A:Acceptor or I:Initiator or q:quit) = ");
      char  strbuf[f9fix::FixRecorder::kMaxFixMsgBufferSize];
      auto  connectionType{fgetstr(strbuf, sizeof(strbuf), stdin)};
      if (connectionType.begin() == nullptr)
         break;
      switch (connectionType.Get1st()) {
      case 'A': TestCommandFixSession(ConnectionType::Acceptor);  break;
      case 'I': TestCommandFixSession(ConnectionType::Initiator); break;
      }
   }
}
