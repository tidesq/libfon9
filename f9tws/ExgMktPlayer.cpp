// \file f9tws/ExgMktPlayer.cpp
// \author fonwinz@gmail.com
#include "f9tws/ExgMktFeeder.hpp"
#include "fon9/seed/Plugins.hpp"
#include "fon9/buffer/DcQueueList.hpp"
#include "fon9/buffer/FwdBufferList.hpp"
#include "fon9/framework/IoFactory.hpp"
#include "fon9/DefaultThreadPool.hpp"
#include "fon9/File.hpp"

namespace f9tws {

struct ExgMktPlayerArgs {
   fon9::File::PosType  RdFrom_{0};
   fon9::File::PosType  RdTo_{0};
   uint64_t             RdBytes_{0};
   fon9::TimeInterval   RdInterval_{fon9::TimeInterval_Millisecond(1)};
};
class ExgMktPlayer : public ExgMktFeeder, public fon9::io::Session {
   fon9_NON_COPY_NON_MOVE(ExgMktPlayer);
   fon9::io::Device*    Device_{nullptr};
   fon9::DcQueueList    RdBuffer_;
   fon9::File           MktFile_;
   ExgMktPlayerArgs     Args_;
   fon9::TimeStamp      LastStTime_;
   uint64_t             SentPkCount_{0};

   struct RdTimer : public fon9::DataMemberTimer {
      fon9_NON_COPY_NON_MOVE(RdTimer);
      RdTimer() = default;
      void EmitOnTimer(fon9::TimeStamp now) override {
         ExgMktPlayer& rthis = fon9::ContainerOf(*this, &ExgMktPlayer::RdTimer_);
         rthis.ReadMktFile(now);
         intrusive_ptr_release(rthis.Device_);
      }
   };
   RdTimer RdTimer_;

   struct NodeSend : public fon9::BufferNodeVirtual {
      fon9_NON_COPY_NON_MOVE(NodeSend);
      using base = fon9::BufferNodeVirtual;
      friend class fon9::BufferNode;// for BufferNode::Alloc();
      ExgMktPlayer* Player_{nullptr};
      using base::base;
   protected:
      void OnBufferConsumed() override {
         // 因為此時正鎖定 SendBuffer & OpQueue,
         // 所以在這裡呼叫 SendBuffered() 或 OpQueue 會死結!
         // 因此將 player->ReadMktFile(); 移到 ThreadPool 處理.
         if (auto player = this->Player_) {
            intrusive_ptr_add_ref(player->Device_);
            //fon9::GetDefaultThreadPool().EmplaceMessage([player]() {
            //   player->ReadMktFile(fon9::UtcNow());
            //   intrusive_ptr_release(player->Device_);
            //});
            player->RdTimer_.RunAfter(player->Args_.RdInterval_);
         }
      }
      void OnBufferConsumedErr(const fon9::ErrC&) override {}
   public:
      static void Send(ExgMktPlayer& player, const void* pk, unsigned pksz) {
         fon9::BufferList buf;
         fon9::AppendToBuffer(buf, pk, pksz);
         buf.push_back(base::Alloc<NodeSend>(0, StyleFlag{}));
         player.Device_->SendBuffered(std::move(buf));
      }
      static void Wait(ExgMktPlayer& player) {
         fon9::BufferList buf;
         NodeSend*        node;
         buf.push_back(node = base::Alloc<NodeSend>(0, StyleFlag{}));
         node->Player_ = &player;
         player.Device_->SendBuffered(std::move(buf));
      }
   };

   void OnDevice_Initialized(fon9::io::Device& dev) override {
      this->Device_ = &dev;
   }
   fon9::io::RecvBufferSize OnDevice_LinkReady(fon9::io::Device&) {
      this->ReadMktFile(fon9::UtcNow());
      return fon9::io::RecvBufferSize::NoRecvEvent;
   }
   void ExgMktOnReceived(const ExgMktHeader& pk, unsigned pksz) override {
      ++this->SentPkCount_;
      NodeSend::Send(*this, &pk, pksz);
   }
   void ReadMktFile(fon9::TimeStamp now) {
      if (this->Device_->OpImpl_GetState() != fon9::io::State::LinkReady)
         return;
      if (this->Args_.RdTo_ != 0 && this->Args_.RdTo_ <= this->Args_.RdFrom_) {
         std::string stmsg = fon9::RevPrintTo<std::string>("Pk=", this->SentPkCount_, "|Pos=", this->Args_.RdFrom_, "|End=", this->Args_.RdTo_);
         this->Device_->Manager_->OnSession_StateUpdated(*this->Device_, &stmsg, fon9::LogLevel::Info);
         return;
      }
      auto node = fon9::FwdBufferNode::Alloc(this->Args_.RdBytes_ ? this->Args_.RdBytes_ : 1024);
      auto rdsz = this->Args_.RdBytes_ ? this->Args_.RdBytes_ : node->GetRemainSize();
      if (this->Args_.RdTo_ != 0) {
         auto remain = this->Args_.RdTo_ - this->Args_.RdFrom_;
         if (rdsz > remain)
            rdsz = remain;
      }
      auto rdres = this->MktFile_.Read(this->Args_.RdFrom_, node->GetDataEnd(), rdsz);
      if (rdres.HasResult() && rdres.GetResult() > 0) {
         this->Args_.RdFrom_ += rdres.GetResult();
         node->SetDataEnd(node->GetDataEnd() + rdres.GetResult());
         this->RdBuffer_.push_back(node);
         this->FeedBuffer(this->RdBuffer_);
         if (now - this->LastStTime_ >= fon9::TimeInterval_Second(1)) {
            this->LastStTime_ = now;
            std::string stmsg = fon9::RevPrintTo<std::string>("Pk=", this->SentPkCount_, "|Pos=", this->Args_.RdFrom_);
            this->Device_->Manager_->OnSession_StateUpdated(*this->Device_, &stmsg, fon9::LogLevel::Trace);
         }
         NodeSend::Wait(*this);
      }
      else {
         fon9::FreeNode(node);
         std::string stmsg = fon9::RevPrintTo<std::string>("Pk=", this->SentPkCount_, "|Pos=", this->Args_.RdFrom_, "|Rd=", rdres);
         this->Device_->Manager_->OnSession_StateUpdated(*this->Device_, &stmsg, fon9::LogLevel::Info);
      }
   }
public:
   ExgMktPlayer(fon9::File&& fd, const ExgMktPlayerArgs& args)
      : MktFile_{std::move(fd)}
      , Args_(args) {
   }
   ~ExgMktPlayer() {
   }
};

class ExgMktPlayerFactory : public fon9::SessionFactory {
   fon9_NON_COPY_NON_MOVE(ExgMktPlayerFactory);
   using base = fon9::SessionFactory;
public:
   using base::base;

   static fon9::io::SessionSP CreateSession(fon9::StrView cfg, std::string& errReason) {
      // cfg.SessionArgs_: fileName|From=pos|To=pos|Rd=BlockSize|Interval=ti|
      // TODO: std::string ExgMktPlayer::SessionCommand(Device& dev, StrView cmdln);
      //    - pause
      //    - restart [from to speed]
      fon9::StrView     fn = fon9::StrFetchTrim(cfg, '|');
      ExgMktPlayerArgs  args;
      fon9::StrView     tag, value;
      while (fon9::StrFetchTagValue(cfg, tag, value)) {
         if (tag == "From")
            args.RdFrom_ = fon9::StrTo(value, args.RdFrom_);
         else if (tag == "To")
            args.RdTo_ = fon9::StrTo(value, args.RdTo_);
         else if (tag == "Rd")
            args.RdBytes_ = fon9::StrTo(value, args.RdBytes_);
         else if (tag == "Interval")
            args.RdInterval_ = fon9::StrTo(value, args.RdInterval_);
         else {
            errReason = fon9::RevPrintTo<std::string>("Create:TwsExgMktPlayer|err=Unknown tag: ", tag);
            return fon9::io::SessionSP{};
         }
      }
      fon9::File fd;
      auto       res = fd.Open(fn.ToString(), fon9::FileMode::Read);
      if (!res) {
         errReason = fon9::RevPrintTo<std::string>("Create:TwsExgMktPlayer|fname=", fd.GetOpenName(), '|', res);
         return fon9::io::SessionSP{};
      }
      return fon9::io::SessionSP{new ExgMktPlayer{std::move(fd), args}};
   }
   fon9::io::SessionSP CreateSession(fon9::IoManager& mgr, const fon9::IoConfigItem& cfg, std::string& errReason) {
      (void)mgr;
      return this->CreateSession(fon9::ToStrView(cfg.SessionArgs_), errReason);
   }
   struct Server : public fon9::io::SessionServer {
      fon9_NON_COPY_NON_MOVE(Server);
      std::string Config_;
      Server(std::string cfg) : Config_{std::move(cfg)} {
      }
      fon9::io::SessionSP OnDevice_Accepted(fon9::io::DeviceServer&) override {
         std::string errReason;
         return ExgMktPlayerFactory::CreateSession(&this->Config_, errReason);
      }
   };
   fon9::io::SessionServerSP CreateSessionServer(fon9::IoManager& mgr, const fon9::IoConfigItem& cfg, std::string& errReason) {
      (void)mgr; (void)errReason;
      return fon9::io::SessionServerSP{new Server(cfg.SessionArgs_.ToString())};
   }
};

static bool TwsExgMktPlayer_Start(fon9::seed::PluginsHolder& holder, fon9::StrView args) {
   (void)args; // plugins args: "Name=ExgMktPlayer|AddTo=FpSession"
   struct ArgsParser : public fon9::SessionFactoryConfigParser {
      using base = fon9::SessionFactoryConfigParser;
      ArgsParser() : base{"TwsExgMktPlayer"} {
      }
      fon9::SessionFactorySP CreateSessionFactory() override {
         return fon9::SessionFactorySP{new ExgMktPlayerFactory{this->Name_}};
      }
   };
   return ArgsParser{}.Parse(holder, args);
}

} // namespaces

extern "C" f9tws_API fon9::seed::PluginsDesc f9p_TwsExgMktPlayer;
fon9::seed::PluginsDesc f9p_TwsExgMktPlayer{"", &f9tws::TwsExgMktPlayer_Start, nullptr, nullptr,};
static fon9::seed::PluginsPark f9pRegister{"TwsExgMktPlayer", &f9p_TwsExgMktPlayer};
