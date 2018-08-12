// \file fon9/seed/fon9Co.cpp
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
static BOOL WINAPI WindowsCtrlBreakHandler(DWORD dwCtrlType) {
   switch (dwCtrlType) {
   case CTRL_C_EVENT:         AppBreakMsg_ = "<Ctrl-C>";      break;// Handle the CTRL-C signal.
   case CTRL_CLOSE_EVENT:     AppBreakMsg_ = "<Close>";       break;// CTRL-CLOSE: confirm that the user wants to exit.
   case CTRL_BREAK_EVENT:     AppBreakMsg_ = "<Ctrl-Break>";  break;// Pass other signals to the next handler.
   case CTRL_LOGOFF_EVENT:    AppBreakMsg_ = "<Logoff>";      break;
   case CTRL_SHUTDOWN_EVENT:  AppBreakMsg_ = "<Shutdown>";    break;
   default:                   AppBreakMsg_ = "<Unknow Ctrl>"; break;
   }
   FILE* fstdin = nullptr;
   freopen_s(&fstdin, "NUL:", "r", stdin);
   return TRUE;
}
#else
#include <signal.h>
#include <sys/mman.h> // mlockall()
static void UnixSignalTermHandler(int) {
   AppBreakMsg_ = "<Signal TERM>";
   if (!freopen("/dev/null", "r", stdin)) {
   }
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
   void OnRequestError(const Request& req, fon9::DcQueue&& errmsg) override {
      (void)req;
      this->WriteToConsole(std::move(errmsg));
      this->Wakeup();
   }
   void OnRequestDone(const Request& req, fon9::DcQueue&& extmsg) override {
      (void)req;
      if (!extmsg.empty())
         this->WriteToConsole(std::move(extmsg));
      this->Wakeup();
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
         if (cmdln == "quit")
            return State::QuitApp;
         if (cmdln == "exit")
            return State::UserExit;
         this->FeedLine(cmdln);
         this->Wait();
      }
   }

public:
   ConsoleSeedSession(fon9::Framework& fon9sys, bool isAdminMode)
      : base(fon9sys.Root_, fon9sys.MaAuth_, isAdminMode) {
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
      return this->AuthUser(fon9::StrTrim(&authz), authc, fon9::StrView{passbuf, passlen}, "console");
   }

   State RunLoop() {
      this->SetCurrPathToHome();
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
   fon9::Framework   fon9sys;
   fon9sys.Initialize(argc, argv);
   fon9::auth::PlantScramSha256(*fon9sys.MaAuth_);
   fon9::auth::PolicyAclAgent::Plant(*fon9sys.MaAuth_);
   fon9sys.Start();

#ifdef fon9_WINDOWS
   SetConsoleCtrlHandler(&WindowsCtrlBreakHandler, TRUE);
#else
   signal(SIGTERM, &UnixSignalTermHandler);
   signal(SIGINT,  &UnixSignalTermHandler);
#endif

   // 使用 "--admin" 啟動 AdminMode.
   bool isAdminMode = (fon9::GetCmdArg(argc, argv, fon9::StrView{}, "admin").begin() != nullptr);
   using SeedSessionSP = fon9::intrusive_ptr<ConsoleSeedSession>;
   SeedSessionSP             coSession{new ConsoleSeedSession{fon9sys, isAdminMode}};
   fon9::SeedSession::State  res = ConsoleSeedSession::State::UserExit;
   if (isAdminMode) {
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
         return 0;
      default:
      case ConsoleSeedSession::State::Authing:
      case ConsoleSeedSession::State::Logouting:
      case ConsoleSeedSession::State::Broken: // stdin EOF, wait signal for quit app.
         coSession->Wait();
         res = coSession->GetState();
         break;;
      }
   }
   puts(AppBreakMsg_);
   return 0;
}
