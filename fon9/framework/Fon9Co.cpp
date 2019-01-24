// \file fon9/framework/fon9Co.cpp
// fon9 console
// \author fonwinz@gmail.com
#include "fon9/framework/Framework.hpp"
#include "fon9/framework/SeedSession.hpp"
#include "fon9/auth/SaslScramSha256Server.hpp"
#include "fon9/auth/SaslClient.hpp"
#include "fon9/auth/PolicyAcl.hpp"
#include "fon9/ConsoleIO.hpp"
#include "fon9/CountDownLatch.hpp"
#include "fon9/CmdArgs.hpp"
#include "fon9/Log.hpp"

#include "fon9/web/HttpSession.hpp"
#include "fon9/web/HttpHandlerStatic.hpp"
#include "fon9/web/HttpDate.hpp"
#include "fon9/web/WsSeedVisitor.hpp"

//--------------------------------------------------------------------------//

#ifdef __GNUC__
#include <mcheck.h>
static void prepare_mtrace(void) __attribute__((constructor));
static void prepare_mtrace(void) { mtrace(); }

// 在 int main() 的 cpp 裡面: 加上這個永遠不會被呼叫的 fn,
// 就可解決 g++: "Enable multithreading to use std::thread: Operation not permitted" 的問題了!
void FixBug_use_std_thread(pthread_t* thread, void *(*start_routine) (void *)) {
   pthread_create(thread, NULL, start_routine, NULL);
}
#endif

//--------------------------------------------------------------------------//

static const char* AppBreakMsg_ = nullptr;

#ifdef fon9_WINDOWS
fon9::ThreadId::IdType mainThreadId;
static BOOL WINAPI WindowsCtrlBreakHandler(DWORD dwCtrlType) {
   switch (dwCtrlType) {
   case CTRL_C_EVENT:         AppBreakMsg_ = "<Ctrl-C>";      break;// Handle the CTRL-C signal.
   case CTRL_BREAK_EVENT:     AppBreakMsg_ = "<Ctrl-Break>";  break;// Pass other signals to the next handler.
   case CTRL_CLOSE_EVENT:     AppBreakMsg_ = "<Close>";       break;// CTRL-CLOSE: confirm that the user wants to exit.
   case CTRL_LOGOFF_EVENT:    AppBreakMsg_ = "<Logoff>";      break;
   case CTRL_SHUTDOWN_EVENT:  AppBreakMsg_ = "<Shutdown>";    break;
   default:                   AppBreakMsg_ = "<Unknow Ctrl>"; break;
   }
   //FILE* fstdin = nullptr;
   //freopen_s(&fstdin, "NUL:", "r", stdin); // 如果遇到 CTRL_CLOSE_EVENT(或其他?) 會卡死在這兒!?
   //所以改用 CancelIoEx() 一樣可以強制中斷 gets();
   CancelIoEx(GetStdHandle(STD_INPUT_HANDLE), nullptr);
   // 雖然 [MSDN](https://docs.microsoft.com/en-us/windows/console/handlerroutine)
   // 說: When the signal is received, the system creates a new thread in the process to execute the function.
   // 但這裡還是多此一舉的判斷一下 (this thread 不是 main thread 才要等 main thread 做完).
   if (mainThreadId != fon9::GetThisThreadId().ThreadId_) {
      do {
         CancelIoEx(GetStdHandle(STD_INPUT_HANDLE), nullptr);
         std::this_thread::sleep_for(std::chrono::milliseconds(20));
      } while (AppBreakMsg_);
      // 因為從這裡返回後 Windows 會直接呼叫 ExitProcess(); 造成 main thread 事情還沒做完.
      // 所以這裡直接結束 this thread, 讓 main thread 可以正常結束, 而不是 ExitProcess(); 強制結束程式.
      ExitThread(0);
   }
   return TRUE;
}
static void SetupCtrlBreakHandler() {
   mainThreadId = fon9::GetThisThreadId().ThreadId_;
   SetConsoleCP(CP_UTF8);
   SetConsoleOutputCP(CP_UTF8);
   SetConsoleCtrlHandler(&WindowsCtrlBreakHandler, TRUE);
}
#else
#include <signal.h>
static void UnixSignalTermHandler(int) {
   AppBreakMsg_ = "<Signal TERM>";
   if (!freopen("/dev/null", "r", stdin)) {
      // Linux 可透過 freopen() 中斷執行中的 fgets();
   }
}
static void SetupCtrlBreakHandler() {
   signal(SIGTERM, &UnixSignalTermHandler);
   signal(SIGINT, &UnixSignalTermHandler);
}
#endif

//--------------------------------------------------------------------------//

class ConsoleSeedSession : public fon9::SeedSession {
   fon9_NON_COPY_NON_MOVE(ConsoleSeedSession);
   using base = fon9::SeedSession;
   fon9::CountDownLatch Waiter_{1};

   void OnAuthEventInLocking(State, fon9::DcQueue&& msg) override {
      this->WriteToConsole(std::move(msg));
      this->Wakeup();
   }
   void OnRequestError(const fon9::seed::TicketRunner&, fon9::DcQueue&& errmsg) override {
      this->WriteToConsole(std::move(errmsg));
      this->Wakeup();
   }
   void OnRequestDone(const fon9::seed::TicketRunner&, fon9::DcQueue&& extmsg) override {
      if (!extmsg.empty())
         this->WriteToConsole(std::move(extmsg));
      this->Wakeup();
   }
   void OnSeedNotify(fon9::seed::VisitorSubr& subr, const fon9::seed::SeedNotifyArgs& args) override {
      fon9::RevBufferList rbuf{256};
      RevPutChar(rbuf, '\n');
      switch (args.NotifyType_) {
      case fon9::seed::SeedNotifyArgs::NotifyType::PodRemoved:
         RevPrint(rbuf, "|PodRemoved");
         break;
      case fon9::seed::SeedNotifyArgs::NotifyType::SeedChanged:
         RevPrint(rbuf, args.GetGridView());
         RevPrint(rbuf, '/', args.KeyText_, '^', args.Tab_->Name_, '|');
         break;
      case fon9::seed::SeedNotifyArgs::NotifyType::SeedRemoved:
         RevPrint(rbuf, '/', args.KeyText_, '^', args.Tab_->Name_, "|SeedRemoved");
         break;
      case fon9::seed::SeedNotifyArgs::NotifyType::ParentSeedClear:
         RevPrint(rbuf, "|OnParentSeedClear");
         break;
      case fon9::seed::SeedNotifyArgs::NotifyType::TableChanged:
         RevPrint(rbuf, "/^", args.Tab_->Name_, "|TableChanged");
         break;
      }
      RevPrint(rbuf, "\n" "SeedNotify|path=", subr.GetPath());
      this->WriteToConsole(fon9::DcQueueList{rbuf.MoveOut()});
   }

   void PutNewLineConsole() {
      fputs("\n", stdout);
   }
   void WritePrompt(fon9::StrView msg) {
      fwrite(msg.begin(), msg.size(), 1, stdout);
      fflush(stdout);
   }
   void WriteToConsole(fon9::DcQueue&& errmsg) {
      fon9::DeviceOutputBlock(std::move(errmsg), [](const void* buf, size_t sz) {
         fwrite(buf, sz, 1, stdout);
         return fon9::Outcome<size_t>{sz};
      });
      this->PutNewLineConsole();
   }

   uint16_t GetDefaultGridViewRowCount() override {
      fon9::winsize wsz;
      fon9::GetConsoleSize(wsz);
      return static_cast<uint16_t>(wsz.ws_row);
   }

   State RunLoopImpl() {
      char        cmdbuf[1024 * 4];
      std::string prompt;
      for (;;) {
         this->GetPrompt(prompt);
         this->WritePrompt(&prompt);
         // TODO: fgets() 可以考慮使用 gnu readline library.
         if (!fgets(cmdbuf, sizeof(cmdbuf), stdin))
             return State::Broken;
         fon9::StrView cmdln{fon9::StrView_cstr(cmdbuf)};
         if (StrTrim(&cmdln).empty())
            continue;
         if (cmdln == "quit") {
            fon9_LOG_IMP("main.quit|user=", this->GetAuthr().GetUserId());
            return State::QuitApp;
         }
         if (cmdln == "exit")
            return State::UserExit;
         this->FeedLine(cmdln);
         this->Wait();
      }
   }

public:
   std::string ConsoleId_;
   ConsoleSeedSession(fon9::Framework& fon9sys)
      : base(fon9sys.Root_, fon9sys.MaAuth_,
             fon9::RevPrintTo<std::string>("console:", fon9::LocalHostId_))
      , ConsoleId_{GetUFrom()} {
   }

   void Wakeup() {
      this->Waiter_.ForceWakeUp();
   }
   void Wait() {
      this->Waiter_.Wait();
      this->Waiter_.AddCounter(1);
   }

   State ReadUser() {
      this->WritePrompt("\n" "Fon9Co login: ");
      char  userbuf[256];
      if (!fgets(userbuf, sizeof(userbuf), stdin))
         return State::Broken;
      fon9::StrView  authc{fon9::StrView_cstr(userbuf)};
      if (fon9::StrTrim(&authc).empty())
         return State::UserExit;

      char     passbuf[1024];
      size_t   passlen = fon9::getpass("Password: ", passbuf, sizeof(passbuf));
      this->PutNewLineConsole();
      fon9::StrView  authz{};
      return this->AuthUser(fon9::StrTrim(&authz), authc, fon9::StrView{passbuf, passlen}, fon9::ToStrView(this->ConsoleId_));
   }

   State RunLoop() {
      this->SetCurrPath(ToStrView(this->Fairy_->GetCurrPath()));
      State st = this->RunLoopImpl();
      this->Logout();
      return st;
   }
};

//--------------------------------------------------------------------------//

int main(int argc, char** argv) {
   #if defined(_MSC_VER) && defined(_DEBUG)
      _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
      //_CrtSetBreakAlloc(176);
   #endif

   #if defined(NDEBUG)
      fon9::LogLevel_ = fon9::LogLevel::Info;
   #endif

   SetupCtrlBreakHandler();

   fon9::Framework   fon9sys;
   fon9sys.Initialize(argc, argv);

   // PlantScramSha256(), PolicyAclAgent::Plant() 要用 plugins 機制嗎?
   fon9::auth::PlantScramSha256(*fon9sys.MaAuth_);
   fon9::auth::PolicyAclAgent::Plant(*fon9sys.MaAuth_);

// ---------------------------------------------------------------
// 若要提供 http 管理介面, 載入必要的 plugins, 底下的設定都是立即生效:
// 建立 TcpServer factory, 放在 DeviceFactoryPark=FpDevice
//    >ss,Enabled=Y,EntryName=TcpServer,Args='Name=TcpServer|AddTo=FpDevice' /MaPlugins/DevTcps
// 建立 Http request 靜態分配器, wwwroot/HttpStatic.cfg 可參考 fon9/web/HttpStatic.cfg:
//    >ss,Enabled=Y,EntryName=HttpStaticDispatcher,Args='Name=HttpRoot|Cfg=wwwroot/HttpStatic.cfg' /MaPlugins/Http-R
// 建立 Http session factory, 使用剛剛建立的 http 分配器(HttpRoot), 放在 SessionFactoryPark=FpSession
//    >ss,Enabled=Y,EntryName=HttpSession,Args='Name=HttpMa|HttpDispatcher=HttpRoot|AddTo=FpSession' /MaPlugins/HttpSes
// 建立 WsSeedVisitor(透過 web service 管理 fon9::seed), 使用 AuthMgr=MaAuth 檢核帳密權限, 放在剛剛建立的 http 分配器(HttpRoot)
//    >ss,Enabled=Y,EntryName=WsSeedVisitor,Args='Name=WsSeedVisitor|AuthMgr=MaAuth|AddTo=HttpRoot' /MaPlugins/HttpWsMa
// 建立 io 管理員, DeviceFactoryPark=FpDevice; SessionFactoryPark=FpSession; 設定儲存在 fon9cfg/MaIo.f9gv
//    >ss,Enabled=Y,EntryName=NamedIoManager,Args='Name=MaIo|DevFp=FpDevice|SesFp=FpSession|Cfg=MaIo.f9gv|SvcCfg="ThreadCount=2|Capacity=100"' /MaPlugins/MaIo
//
// 在 io 管理員(MaIo) 設定 HttpSession:
// 建立設定, 此時尚未套用(也尚未存檔):
//    >ss,Enabled=Y,Session=HttpMa,Device=TcpServer,DeviceArgs=6080 /MaIo/^edit:Config/MaHttp
// 查看設定及套用時的 SubmitId:
//    >gv /MaIo/^apply:Config
// 套用設定, 假設上一行指令的結果提示 SubmitId=123:
//    >/MaIo/^apply:Config submit 123
// ---------------------------------------------------------------

   fon9sys.Start();

   using SeedSessionSP = fon9::intrusive_ptr<ConsoleSeedSession>;
   SeedSessionSP             coSession{new ConsoleSeedSession{fon9sys}};
   fon9::SeedSession::State  res = ConsoleSeedSession::State::UserExit;

   // 使用 "--admin" 啟動 AdminMode.
   if (fon9::GetCmdArg(argc, argv, fon9::StrView{}, "admin").begin() != nullptr) {
      coSession->SetAdminMode();
      puts("Fon9Co admin mode.");
      res = coSession->RunLoop();
   }

   while (AppBreakMsg_ == nullptr) {
      switch (res) {
      case ConsoleSeedSession::State::None:
      case ConsoleSeedSession::State::AuthError:
      case ConsoleSeedSession::State::UserExit:
         res = coSession->ReadUser();
         break;
      case ConsoleSeedSession::State::UserReady:
         res = coSession->RunLoop();
         break;
      case ConsoleSeedSession::State::QuitApp:
         AppBreakMsg_ = "Normal QuitApp";
         break;
      default:
      case ConsoleSeedSession::State::Authing:
      case ConsoleSeedSession::State::Logouting:
         coSession->Wait();
         res = coSession->GetState();
         break;
      case ConsoleSeedSession::State::Broken:
         // stdin EOF, sleep() and retry read.
         // wait signal for quit app.
         // 一旦遇到 EOF, 就需要重新登入?
         int c;
         do {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            clearerr(stdin);
            c = fgetc(stdin);
         } while (c == EOF && AppBreakMsg_ == nullptr);
         ungetc(c, stdin);
         res = coSession->GetState();
         continue;
      }
   }
   fon9_LOG_IMP("main.quit|cause=console:", AppBreakMsg_);
   puts(AppBreakMsg_);
   AppBreakMsg_ = nullptr;
   fon9sys.DisposeForAppQuit();
   return 0;
}
