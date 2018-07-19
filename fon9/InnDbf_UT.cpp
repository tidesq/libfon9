// \file fon9/InnDbf_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "fon9/TestTools.hpp"
#include "fon9/InnDbf.hpp"
#include "fon9/InnSyncerFile.hpp"
#include "fon9/BitvArchive.hpp"
#include "fon9/Log.hpp"
#include "fon9/DefaultThreadPool.hpp"
#include "fon9/Timer.hpp"
#include <map>
#include <thread>

//--------------------------------------------------------------------------//

static const char kInnSyncInFileName[] = "SynIn.log";
static const char kInnSyncOutFileName[] = "SynOut.log";
static const char kDbfFileName1[] = "Dbf1.inn";
static const char kDbfFileName2[] = "Dbf2.inn";

void RemoveTestFiles() {
   remove(kInnSyncInFileName);
   remove(kInnSyncOutFileName);
   remove(kDbfFileName1);
   remove(kDbfFileName2);
}

//--------------------------------------------------------------------------//

struct UserRec {
   fon9::CharVector  Name_;
   fon9::CharVector  RoleId_;
   fon9::TimeStamp   LastAuthTime_ = fon9::TimeStamp::Null();

   inline friend bool operator==(const UserRec& v1, const UserRec& v2) {
      return v1.Name_ == v2.Name_
         && v1.RoleId_ == v2.RoleId_
         && v1.LastAuthTime_ == v2.LastAuthTime_;
   }
};

template <class Archive>
void Serialize(Archive& ar, fon9::ArchiveWorker<Archive, UserRec>& rec) {
   ar(rec.Name_, rec.RoleId_, rec.LastAuthTime_);
}

class UserTable : public fon9::InnDbfTableHandler {
   fon9_NON_COPY_NON_MOVE(UserTable);
public:
   UserTable() = default;

   struct User : public UserRec {
      fon9_NON_COPYABLE(User);
      User() = default;
      fon9::InnDbfRoomKey  RoomKey_;
   };
   using UserId = fon9::CharVector;
   using UserMap = std::map<UserId, User>;
   using DeletedMap = std::map<fon9::CharVector, fon9::InnDbfRoomKey>;

   /// 若 userId 不存在則新增, 若已存在則更新.
   void Set(const UserId& userId, const UserRec& user) {
      Map::Locker maps{this->Map_};
      User&       rec = maps->UserMap_[userId];
      auto        idel = maps->DeletedMap_.find(userId);
      if (idel != maps->DeletedMap_.end()) {
         assert(!rec.RoomKey_);
         rec.RoomKey_ = std::move(idel->second);
         maps->DeletedMap_.erase(idel);
      }
   
      *static_cast<UserRec*>(&rec) = user;
      fon9::RevBufferList  rbuf{sizeof(User) + sizeof(UserId)};
      fon9::BitvOutArchive{rbuf}(userId, rec);
      this->WriteRoom(rec.RoomKey_, nullptr, fon9::InnDbfRoomType::RowData, rbuf.MoveOut());
   }
   void Delete(const UserId& userId) {
      Map::Locker maps{this->Map_};
      auto        irec = maps->UserMap_.find(userId);
      if (irec == maps->UserMap_.end())
         return;
      auto& droom = maps->DeletedMap_[userId];
      assert(!droom);
      droom = std::move(irec->second.RoomKey_);
      maps->UserMap_.erase(irec);

      fon9::RevBufferList rbuf{sizeof(User) + sizeof(UserId)};
      fon9::ToBitv(rbuf, userId);
      this->WriteRoom(droom, nullptr, fon9::InnDbfRoomType::RowDeleted, rbuf.MoveOut());
   }
   
   inline static bool DeletedMapIsEqual(const DeletedMap& v1, const DeletedMap& v2) {
      if (v1.size() != v2.size())
         return false;
      auto i = v2.begin();
      for (auto& v : v1) {
         if (v.first != i->first)
            return false;
         ++i;
      }
      return true;
   }
   bool IsEqual(const UserTable& rhs) const {
      Map::ConstLocker maps1{this->Map_};
      Map::ConstLocker maps2{rhs.Map_};
      return maps1->UserMap_ == maps2->UserMap_
          && DeletedMapIsEqual(maps1->DeletedMap_, maps2->DeletedMap_);
   }
   bool IsDeletedEmpty() const {
      Map::ConstLocker maps{this->Map_};
      return maps->DeletedMap_.empty();
   }
   bool IsEqualUserMap(const UserTable& rhs) const {
      Map::ConstLocker maps1{this->Map_};
      Map::ConstLocker maps2{rhs.Map_};
      return maps1->UserMap_ == maps2->UserMap_;
   }
   bool IsEqualRoomCount() const {
      Map::ConstLocker maps{this->Map_};
      return (maps->UserMap_.size() + maps->DeletedMap_.size()) == this->GetRoomCount();
   }
   void Clear() {
      Map::Locker maps{this->Map_};
      maps->UserMap_.clear();
      maps->DeletedMap_.clear();
   }

private:
   struct MapImpl {
      UserMap     UserMap_;
      DeletedMap  DeletedMap_;
   };
   using Map = fon9::MustLock<MapImpl>;
   Map   Map_;

   struct HandlerBase {
      UserId   Key_;
      static fon9::InnDbfRoomKey& GetRoomKey(DeletedMap::value_type& v) {
         return v.second;
      }
      static fon9::InnDbfRoomKey& GetRoomKey(UserMap::value_type& v) {
         return v.second.RoomKey_;
      }
   };

   struct LoadHandler : public HandlerBase, public fon9::InnDbfLoadHandler {
      fon9_NON_COPY_NON_MOVE(LoadHandler);
      using InnDbfLoadHandler::InnDbfLoadHandler;
      void UpdateLoad(UserMap& userMap, UserMap::iterator* iuser) {
         User* rec;
         if (iuser)
            rec = &((*iuser)->second);
         else
            rec = &userMap[this->Key_];
         rec->RoomKey_ = std::move(this->EvArgs_.RoomKey_);
         fon9::BitvInArchive{*this->EvArgs_.Buffer_}(*rec);
      }
      void UpdateLoad(DeletedMap& deletedMap, DeletedMap::iterator* ideleted) {
         if (ideleted)
            (*ideleted)->second = std::move(this->EvArgs_.RoomKey_);
         else
            deletedMap[this->Key_] = std::move(this->EvArgs_.RoomKey_);
      }
   };
   virtual void OnInnDbfTable_Load(fon9::InnDbfLoadEventArgs& e) override {
      LoadHandler handler{*this, e};
      BitvTo(*e.Buffer_, handler.Key_);

      Map::Locker maps{this->Map_};
      fon9::OnInnDbfLoad(maps->UserMap_, maps->DeletedMap_, handler);
   }

   struct SyncHandler : public HandlerBase, public fon9::InnDbfSyncHandler {
      fon9_NON_COPY_NON_MOVE(SyncHandler);
      SyncHandler(InnDbfTableHandler& handler, fon9::InnDbfSyncEventArgs& e)
         : InnDbfSyncHandler{handler, e} {
      }
      fon9::InnDbfRoomKey* PendingWriteRoomKey_{nullptr};
      fon9::RevBufferList  PendingWriteBuf_{sizeof(User) + sizeof(UserId)};

      void UpdateSync(UserMap& userMap, UserMap::iterator* iuser) {
         assert(this->EvArgs_.RoomType_ == fon9::InnDbfRoomType::RowData);
         User* rec;
         if (iuser)
            rec = &((*iuser)->second);
         else {
            rec = &userMap[this->Key_];
            rec->RoomKey_ = std::move(this->PendingFreeRoomKey_);
         }
         fon9::BitvInArchive{*this->EvArgs_.Buffer_}(*rec);

         this->PendingWriteRoomKey_ = &rec->RoomKey_;
         fon9::BitvOutArchive{this->PendingWriteBuf_}(this->Key_, *rec);
      }
      void UpdateSync(DeletedMap& deletedMap, DeletedMap::iterator* ideleted) {
         assert(this->EvArgs_.RoomType_ == fon9::InnDbfRoomType::RowDeleted);
         if (ideleted)
            this->PendingWriteRoomKey_ = &((*ideleted)->second);
         else {
            this->PendingWriteRoomKey_ = &deletedMap[this->Key_];
            *this->PendingWriteRoomKey_ = std::move(this->PendingFreeRoomKey_);
         }
         fon9::ToBitv(this->PendingWriteBuf_, this->Key_);
      }
   };
   virtual void OnInnDbfTable_Sync(fon9::InnDbfSyncEventArgs& e) override {
      SyncHandler handler(*this, e);
      BitvTo(*e.Buffer_, handler.Key_);
      
      Map::Locker maps{this->Map_};
      fon9::OnInnDbfSync(maps->UserMap_, maps->DeletedMap_, handler);
      if (handler.PendingWriteRoomKey_)
         this->WriteRoom(*handler.PendingWriteRoomKey_, &e.SyncKey_, e.RoomType_, handler.PendingWriteBuf_.MoveOut());
   }

   virtual void OnInnDbfTable_SyncFlushed() override {
      DeletedMap deletedMap;
      {
         Map::Locker maps{this->Map_};
         deletedMap.swap(maps->DeletedMap_);
      }
      for (auto& v : deletedMap)
         this->FreeRoom(std::move(v.second));
   }
};
using UserTableSP = fon9::intrusive_ptr<UserTable>;

//--------------------------------------------------------------------------//

static fon9::TimeInterval  kSyncInInterval = fon9::TimeInterval_Millisecond(10);
static fon9::InnRoomSize   kMinUserRoomSize = 32;
static fon9::InnRoomSize   kInnBlockSize = 64;

struct TestDbf {
   fon9::InnSyncerSP Syncer_;
   fon9::InnDbfSP    Dbf_;
   UserTableSP       UserTable_{new UserTable};

   TestDbf(fon9::StrView dbfFileName, std::string syncOutFileName, std::string syncInFileName)
      : Syncer_{new fon9::InnSyncerFile(fon9::InnSyncerFile::CreateArgs(
                                             syncOutFileName,
                                             syncInFileName,
                                             kSyncInInterval))}
      , Dbf_{new fon9::InnDbf("dbf", Syncer_)} {
      this->OpenLinkLoad(dbfFileName);
      this->Syncer_->StartSync();
   }
   TestDbf() : Dbf_{new fon9::InnDbf("dbf", Syncer_)} {
   }

   ~TestDbf() {
      this->Close();
   }

   void Open(fon9::StrView dbfFileName) {
      fon9::InnDbf::OpenArgs oargs{dbfFileName.ToString(), kInnBlockSize};
      auto res = this->Dbf_->Open(oargs);
      if (!res) {
         fon9_LOG_FATAL("TestDbf|Open=", res);
         abort();
      }
   }
   void OpenLinkLoad(fon9::StrView dbfFileName) {
      this->UserTable_->Clear();
      this->Open(dbfFileName);
      this->Dbf_->LinkTable("User", this->UserTable_, kMinUserRoomSize);
      this->Dbf_->LoadAll();
   }
   void Close() {
      if (this->Syncer_)
         this->Syncer_->StopSync();
      this->Dbf_->DelinkTable(this->UserTable_);
      this->Dbf_->Close();
   }
};

void TestInnDbf() {
   TestDbf dbf1(kDbfFileName1, kInnSyncOutFileName, kInnSyncInFileName);
   TestDbf dbf2(kDbfFileName2, kInnSyncInFileName, kInnSyncOutFileName);

   std::cout << "[TEST ] dbf1(add,modify,delete) => sync => dbf2";

   struct UserId {
      fon9_NON_COPY_NON_MOVE(UserId);
      UserId() = default;
      fon9::RevBufferFixedSize<128> Buf_;
      fon9::CharVector Make(size_t id) {
         this->Buf_.Rewind();
         fon9::RevPrint(this->Buf_, "uid", id, fon9::FmtDef{"08"});
         return fon9::CharVector::MakeRef(this->Buf_.GetCurrent(), this->Buf_.GetMemEnd());
      }
   };
   UserId  userId;
   UserRec user;
   const size_t kCount = 10 * 1000;

   // add user.
   for (size_t L = 0; L < kCount; ++L) {
      user.LastAuthTime_ = fon9::UtcNow();
      user.Name_.assign("Name");
      user.RoleId_.assign("RoleId");
      dbf1.UserTable_->Set(userId.Make(L), user);
   }

   // delete user
   for (size_t L = 0; L < kCount; L += 10)
      dbf1.UserTable_->Delete(userId.Make(L));

   // modify user: 測試更新同一個 room, 或 room 空間不足(重新分配).
   static const char* nameList[] = {
      "TestName.1", "TestName.12", "TestName.123", "TestName.1234", "TestName.12345",
      "TestName.123456", "TestName.1234567", "TestName.12345678", "TestName.123456789", "TestName.1234567890",
   };
   size_t iNameList = 0;
   for (size_t L = 0; L < kCount; L += 3) {
      user.LastAuthTime_ = fon9::UtcNow();
      user.Name_.assign(fon9::ToStrView(nameList[iNameList++ % fon9::numofele(nameList)]));
      dbf1.UserTable_->Set(userId.Make(L), user);
   }

   std::cout << "|WaitFlush" << std::flush;
   dbf1.Dbf_->WaitFlush();

   std::cout << "|WaitSync" << std::flush;
   for (unsigned L = 0; L < 1000; ++L) {
      std::this_thread::sleep_for(kSyncInInterval.ToDuration());
      if (!dbf1.UserTable_->IsEqual(*dbf2.UserTable_))
         continue;

      if (!dbf1.UserTable_->IsEqualRoomCount()) {
         std::cout << "|dbf1.RoomCount";
         break;
      }
      dbf2.Dbf_->WaitFlush(); 
      if (!dbf2.UserTable_->IsEqualRoomCount()) {
         std::cout << "|dbf2.RoomCount";
         break;
      }
      std::cout << "|Done." "\r" "[OK   ]" << std::endl;

      std::cout << "[TEST ] LinkTable() * N";
      for (unsigned tableId = 2; tableId < 1000; ++tableId) {
         dbf1.Dbf_->DelinkTable(dbf1.UserTable_);
         fon9::RevBufferFixedSize<128> tableName;
         fon9::RevPrint(tableName, "Table", tableId, fon9::FmtDef{"08"});
         dbf1.Dbf_->LinkTable(fon9::StrView(tableName.GetCurrent(), tableName.GetMemEnd()), dbf1.UserTable_, kMinUserRoomSize);
      }
      std::cout << "\r" "[OK   ]" << std::endl;

      dbf1.Close();
      dbf2.Close();

      std::cout << "[TEST ] " << kDbfFileName1 << " LoadAll()=>LinkTable()" << std::flush;
      TestDbf test;
      test.Open(kDbfFileName1);
      test.Dbf_->LoadAll();
      test.Dbf_->LinkTable("User", test.UserTable_, kMinUserRoomSize);
      if (!test.UserTable_->IsEqual(*dbf1.UserTable_))
         break;
      if (!test.UserTable_->IsEqualRoomCount()) {
         std::cout << "|RoomCount";
         break;
      }

      test.Close();
      test.OpenLinkLoad(kDbfFileName2);
      std::cout << "|" << kDbfFileName2 << " LinkTable()=>LoadAll()" << std::flush;
      if (!test.UserTable_->IsEqual(*dbf1.UserTable_))
         break;
      if (!test.UserTable_->IsEqualRoomCount()) {
         std::cout << "|RoomCount";
         break;
      }
      std::cout << "\r" "[OK   ]\n";

      struct Aux {
         static bool TestSyncFlushed(TestDbf& test, const TestDbf& dbf1) {
            static_cast<fon9::InnSyncHandler*>(test.Dbf_.get())->OnInnSyncFlushed(*test.Syncer_.get());
            if (!test.UserTable_->IsEqualUserMap(*dbf1.UserTable_) || !test.UserTable_->IsDeletedEmpty())
               return false;
            if (!test.UserTable_->IsEqualRoomCount()) {
               std::cout << "|RoomCount";
               return false;
            }
            return true;
         }
      };
      std::cout << "[TEST ] SyncFlushed";
      Aux::TestSyncFlushed(test, dbf1);

      std::cout << "|Reopen & SyncFlushed";
      test.Close();
      test.OpenLinkLoad(kDbfFileName2);
      Aux::TestSyncFlushed(test, dbf1);

      std::cout << "\r" "[OK   ]" << std::endl;
      return;
   }
   std::cout << "|err=not match." "\r" "[ERROR]" << std::endl;
   abort();
}

//--------------------------------------------------------------------------//

int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
   //_CrtSetBreakAlloc(176);
#endif
   fon9::AutoPrintTestInfo utinfo("InnFile");
   fon9::GetDefaultThreadPool();
   fon9::GetDefaultTimerThread();
   std::this_thread::sleep_for(std::chrono::milliseconds{10});

   RemoveTestFiles();

   TestInnDbf();

   bool isKeepTestFiles = false;
   for (int L = 1; L < argc; ++L) {
      if (strcmp(argv[L], "--keep") == 0)
         isKeepTestFiles = true;
   }
   if (!isKeepTestFiles)
      RemoveTestFiles();
}
