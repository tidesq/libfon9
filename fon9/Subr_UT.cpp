// \file fon9/Subr_UT.cpp
// \author fonwinz@gmail.com
#include "fon9/Subr.hpp"
#include "fon9/TestTools.hpp"
#include <atomic>
#include <thread>
//----------------------------------------------------------------------------
typedef uint64_t        SubrMsg;
std::atomic<uint64_t>   gMsgCount;
#ifdef _DEBUG
static const size_t     kPubTimes = 1000 * 10;
#else
static const size_t     kPubTimes = 1000 * 1000 * 10;
#endif
//----------------------------------------------------------------------------
// 使用最原始的 function pointer 測試效率.
void SubrFunc(const SubrMsg& msg) {
   (void)msg;
   ++gMsgCount;
}
typedef void (*FnPtrSubr)(const SubrMsg& msg);
FnPtrSubr   gFnPtrSubr = &SubrFunc;

void BenchmarkFnPtr(const char* msg) {
   gMsgCount = 0;
   fon9::StopWatch stopWatch;
   for (SubrMsg L = 0; L < kPubTimes; ++L)
      (*gFnPtrSubr)(L);
   stopWatch.PrintResult(msg, kPubTimes);
   assert(gMsgCount == kPubTimes);
}
template <class Subj>
void BenchmarkSubrFnPtr(const char* msg) {
   gMsgCount = 0;
   Subj  subj{1};
   subj.Subscribe(gFnPtrSubr);
   fon9::StopWatch stopWatch;
   for (SubrMsg L = 0; L < kPubTimes; ++L)
      subj.Publish(L);
   stopWatch.PrintResult(msg, kPubTimes);
   assert(gMsgCount == kPubTimes);
}
//----------------------------------------------------------------------------
struct Subr {
   fon9_NOINLINE(void operator()(const SubrMsg& msg)) {
      (void)msg;
      ++gMsgCount;
   }
};
template <class Subj>
void BenchmarkSubrClass(const char* msg) {
   gMsgCount = 0;
   Subr  subr;
   Subj  subj{1};
   subj.Subscribe(subr);
   fon9::StopWatch stopWatch;
   for (SubrMsg L = 0; L < kPubTimes; ++L)
      subj.Publish(L);
   stopWatch.PrintResult(msg, kPubTimes);
   assert(gMsgCount == kPubTimes);
}
//----------------------------------------------------------------------------
// 測試: 註冊&取消.
//   - Thr0: 不斷的發行訊息
//   - ThrA: 不斷的註冊新的訂閱物件A(該訂閱物件收到訊息後,自動在收到訊息的 thread 使用 SubConn 取消註冊
//   - ThrB: 不斷的註冊新的訂閱物件B(該訂閱物件收到訊息後,自動在收到訊息的 thread 使用 Subscriber 取消註冊
//   - ThrC: 不斷的註冊新的訂閱物件C & 取消(使用 SubConn)訂閱
//   - ThrD: 不斷的註冊新的訂閱物件D & 取消(使用 Subscriber)訂閱
void StressSubr() {
   class Subj;
   class SubrBase {
      fon9_NON_COPY_NON_MOVE(SubrBase);
   public:
      Subj&       Subj_;
      std::thread Thr_;
      size_t      MsgCount_ = 0;
      SubrBase(Subj& subj) : Subj_(subj) {
      }
      virtual ~SubrBase() {
         // this->Thr_.join(); 因為 ThrRun 是衍生者實作, 所以應由衍生者呼叫 join();
      }
      virtual void operator()(const SubrMsg& msg) {
         (void)msg;
         ++this->MsgCount_;
      }
   };

   fon9_WARN_DISABLE_PADDING;
   class Subj : public fon9::Subject<fon9::ObjCallback<SubrBase>> {
      fon9_NON_COPY_NON_MOVE(Subj);
      using base = Subject<fon9::ObjCallback<SubrBase>>;
   public:
      std::atomic<bool> IsPubEnd_{false};
      Subj() : base{128} {
      }
      ~Subj() {
         std::cout << "SubscriberCount=" << this->GetSubscriberCount() << "|when Subject dtor." << std::endl;
      }
   };
   fon9_WARN_POP;
   Subj  subject;

   /// - 在 thread 不斷註冊並保留 connection
   /// - 在收到訊息時, 取消已註冊的 connection
   class SubrA : public SubrBase {
      fon9_NON_COPY_NON_MOVE(SubrA);
      typedef std::vector<fon9::SubConn>  SubConns;
      typedef std::lock_guard<std::mutex> Locker;
      std::mutex  Mutex_;
      SubConns    Connections_;
      size_t      MaxConnCount_{};
      size_t      TotConnCount_{};
   public:
      SubrA(Subj& subj) : SubrBase{subj} {
         this->Thr_ = std::thread(&SubrA::ThrRun, this);
      }
      ~SubrA() {
         this->Thr_.join();
         std::cout << "SubrA"
            << "|MsgCount=" << this->MsgCount_
            << "|MaxConnCount=" << this->MaxConnCount_
            << "|TotConnCount=" << this->TotConnCount_
            << std::endl;
      }
      virtual void operator()(const SubrMsg& msg) override {
         (void)msg;
         ++this->MsgCount_;
         SubConns   conns;
         {
            Locker locker(this->Mutex_);
            conns.swap(this->Connections_);
         }
         for(fon9::SubConn c : conns)
            this->Subj_.Unsubscribe(c);
         if (this->MaxConnCount_ < conns.size())
            this->MaxConnCount_ = conns.size();
      }
      void ThrRun() {
         size_t currCount{};
         while (!this->Subj_.IsPubEnd_) {
            // { ===>
            // Locker locker(this->Mutex_);
            // this->Connections_.push_back(this->Subj_.Subscribe(this));
            // ↑ 上面這樣會死結!! 因為:
            //    - 訊息發行的 lock 順序: subject.lock => suba.lock
            //    - 這裡的 lock 順序: suba.lock => subject.lock
            // <=== }
            ++this->TotConnCount_;
            if(currCount > 1000)
               std::this_thread::yield();
            fon9::SubConn c = this->Subj_.Subscribe(this);
            Locker locker(this->Mutex_);
            this->Connections_.push_back(c);
            currCount = this->Connections_.size();
         }
         // subject 已結束發行, 不會再收到訊息了, 取消全部的註冊.
         for (fon9::SubConn c : this->Connections_)
            this->Subj_.Unsubscribe(c);
      }
   };
   SubrA subrA{subject};

   /// - 在 thread 不斷註冊: 不保留 connection
   /// - 在收到訊息時, 使用 this 取消註冊.
   class SubrB : public SubrBase {
      fon9_NON_COPY_NON_MOVE(SubrB);
      size_t   MaxConnCount_{};
      size_t   TotConnCount_{};
      std::atomic<size_t> CurrCount_{};
   public:
      SubrB(Subj& subj) : SubrBase{subj} {
         this->Thr_ = std::thread(&SubrB::ThrRun, this);
      }
      ~SubrB() {
         this->Thr_.join();
         std::cout << "SubrB"
            << "|MsgCount=" << this->MsgCount_
            << "|MaxConnCount=" << this->MaxConnCount_
            << "|TotConnCount=" << this->TotConnCount_
            << std::endl;
      }
      virtual void operator()(const SubrMsg& msg) override {
         (void)msg;
         ++this->MsgCount_;
         size_t count = this->Subj_.UnsubscribeAll(this);
         if (this->MaxConnCount_ < count)
            this->MaxConnCount_ = count;
         this->CurrCount_ = 0;
      }
      void ThrRun() {
         while (!this->Subj_.IsPubEnd_) {
            ++this->TotConnCount_;
            this->Subj_.Subscribe(this);
            if (++this->CurrCount_ > 1000)
               std::this_thread::yield();
         }
         this->Subj_.UnsubscribeAll(this);
      }
   };
   SubrB subrB{subject};

   /// - 在 thread 不斷註冊&取消: 使用 connection
   class SubrC : public SubrBase {
      fon9_NON_COPY_NON_MOVE(SubrC);
   public:
      SubrC(Subj& subj) : SubrBase(subj) {
         this->Thr_ = std::thread(&SubrC::ThrRun, this);
      }
      ~SubrC() {
         this->Thr_.join();
         std::cout << "SubrC|MsgCount=" << this->MsgCount_ << std::endl;
      }
      void ThrRun() {
         while (!this->Subj_.IsPubEnd_) {
            fon9::SubConn c = this->Subj_.Subscribe(this);
            this->Subj_.Unsubscribe(c);
         }
      }
   };
   SubrC subrC{subject};

   /// - 在 thread 不斷註冊&取消: 使用 this
   class SubrD : public SubrBase {
      fon9_NON_COPY_NON_MOVE(SubrD);
   public:
      SubrD(Subj& subj) : SubrBase(subj) {
         this->Thr_ = std::thread(&SubrD::ThrRun, this);
      }
      ~SubrD() {
         this->Thr_.join();
         std::cout << "SubrD|MsgCount=" << this->MsgCount_ << std::endl;
      }
      void ThrRun() {
         while (!this->Subj_.IsPubEnd_) {
            this->Subj_.Subscribe(this);
            this->Subj_.Unsubscribe(this);
         }
      }
   };
   SubrD subrD(subject);

   /// 用 Combiner 計算某次發行時的訂閱者數量.
   struct Combiner {
      size_t Count_{};
      bool operator()(typename Subj::Subscriber& subr, const SubrMsg& msg) {
         ++this->Count_;
         subr(msg);
         return true;
      }
   };
   /// 啟動發行.
   const uint64_t kStressPutTimes = kPubTimes;
   std::cout << "StressSubr|PubCount=" << kStressPutTimes << std::endl;
   size_t maxCombineCount = 0;
   for (uint64_t L = 0; L < kStressPutTimes; ++L) {
      Combiner combine;
      subject.Combine(combine, L);
      if (maxCombineCount < combine.Count_)
         maxCombineCount = combine.Count_;
      if (L % (kStressPutTimes / 100) == 0) {
         std::cout << (100 * L) / kStressPutTimes << "\r";
         fflush(stdout);
      }
   }
   std::cout << "MaxCombineCount=" << maxCombineCount << std::endl;
   subject.IsPubEnd_ = true;
}
//----------------------------------------------------------------------------
int main(int argc, char* argv[])
{
   (void)argc; (void)argv;
   fon9::AutoPrintTestInfo utinfo{"Subr"};

   StressSubr();

   utinfo.PrintSplitter();

   using FnObjSubr = std::function<void(const SubrMsg&)>;
   BenchmarkFnPtr                                    ("function pointer                           ");
   BenchmarkSubrFnPtr<fon9::UnsafeSubject<FnPtrSubr>>("unsafe.Subject<FnPtr>                      ");
   BenchmarkSubrFnPtr<fon9::UnsafeSubject<FnObjSubr>>("unsafe.Subject<std::function(FnPtr)>       ");
   BenchmarkSubrClass<fon9::UnsafeSubject<Subr>>     ("unsafe.Subject<struct Subr>                ");
   BenchmarkSubrClass<fon9::UnsafeSubject<FnObjSubr>>("unsafe.Subject<std::function(struct Subr)> ");

   BenchmarkSubrFnPtr<fon9::Subject<FnPtrSubr>>      ("Subject<FnPtr>                             ");
   BenchmarkSubrFnPtr<fon9::Subject<FnObjSubr>>      ("Subject<std::function(FnPtr)>              ");
   BenchmarkSubrClass<fon9::Subject<Subr>>           ("Subject<struct Subr>                       ");
   BenchmarkSubrClass<fon9::Subject<FnObjSubr>>      ("Subject<std::function(struct Subr)>        ");
}

#ifdef __GNUC__
// 在 int main() 的 cpp 裡面: 加上這個永遠不會被呼叫的 fn,
// 就可解決 g++: "Enable multithreading to use std::thread: Operation not permitted" 的問題了!
void FixBug_use_std_thread(pthread_t* thread, void *(*start_routine) (void *)) {
   pthread_create(thread, NULL, start_routine, NULL);
}
#endif
