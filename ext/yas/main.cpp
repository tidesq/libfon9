#include "fon9/TestTools.hpp"
#include "fon9/buffer/DcQueue.hpp"
#include "fon9/BitvArchive.hpp"
#include "fon9/CharAry.hpp"

#include <yas/mem_streams.hpp>
#include <yas/binary_iarchive.hpp>
#include <yas/binary_oarchive.hpp>
#include <yas/std_types.hpp>

static const unsigned kTimes = 10 * 1000 * 1000;

//--------------------------------------------------------------------------//

void BitvBenchmarkSeq(const char* strHead) {
   size_t          sz = 0;
   char            tempbuffer[1024];
   fon9::StopWatch stopWatch;
   for (unsigned L = 0; L < kTimes; ++L) {
        fon9::RevBufferFixedMem rmem{tempbuffer, sizeof(tempbuffer)};
        sz += fon9::ToBitv(rmem, L);
        fon9::DcQueueFixedMem ibuf{rmem.GetCurrent(), rmem.GetMemEnd()+1};//+1: 讓最後不用呼叫 ByteQuRemoveMore();
        fon9::BitvTo(ibuf, L);
   }
   stopWatch.PrintResultNoEOL(strHead, kTimes) << "|size=" << sz << std::endl;
}

template <size_t opts>
void BenchmarkSeq(const char* strHead) {
   size_t          sz = 0;
   char            tempbuffer[1024];
   fon9::StopWatch stopWatch;
   for (unsigned L = 0; L < kTimes; ++L) {
        yas::mem_ostream os(tempbuffer, sizeof(tempbuffer));
        yas::binary_oarchive<yas::mem_ostream, opts> oa(os);
        oa & L;
        sz += os.get_intrusive_buffer().size;

        yas::mem_istream is(os.get_intrusive_buffer());
        yas::binary_iarchive<yas::mem_istream, opts> ia(is);
        ia & L;
   }
   stopWatch.PrintResultNoEOL(strHead, kTimes) << "|size=" << sz << std::endl;
}

//--------------------------------------------------------------------------//

template <class Record>
void BitvBenchmarkRecord(const char* strHead, const Record& srec) {
   Record          rec{srec};
   size_t          sz = 0;
   char            tempbuffer[1024];
   fon9::StopWatch stopWatch;
   for (unsigned L = 0; L < kTimes; L++) {
        fon9::RevBufferFixedMem rmem{tempbuffer, sizeof(tempbuffer)};
        fon9::BitvOutArchive    oar{rmem};
        oar(rec);
        sz += rmem.GetUsedSize();

        fon9::DcQueueFixedMem ibuf{rmem.GetCurrent(), rmem.GetMemEnd()+1};//+1: 讓最後不用呼叫 ByteQuRemoveMore();
        fon9::BitvInArchive   iar{ibuf};
        iar(rec);
   }
   stopWatch.PrintResultNoEOL(strHead, kTimes) << "|size=" << sz << std::endl;

   sz /= kTimes;
   for (size_t L = 0; L < sz; ++L)
      fprintf(stdout, "%02x ", *reinterpret_cast<fon9::byte*>(tempbuffer + sizeof(tempbuffer) - sz + L));
   fprintf(stdout, "\n");
   if (srec == rec)
      return;
   fprintf(stdout, "\n" "Data not match!\n");
   abort();
}

template <size_t opts, class Record>
void BenchmarkRecord(const char* strHead, const Record& srec) {
   Record          rec{srec};
   char            tempbuffer[1024];
   fon9::StopWatch stopWatch;
   for (unsigned L = 0; L < kTimes; L++) {
        yas::mem_ostream os(tempbuffer, sizeof(tempbuffer));
        yas::binary_oarchive<yas::mem_ostream, opts> oa(os);
        oa & rec;

        yas::mem_istream is(os.get_intrusive_buffer());
        yas::binary_iarchive<yas::mem_istream, opts> ia(is);
        ia & rec;
   }

   yas::mem_ostream os(tempbuffer, sizeof(tempbuffer));
   yas::binary_oarchive<yas::mem_ostream, opts> oa(os);
   oa & rec;
   auto   buf = os.get_intrusive_buffer();
   size_t sz = buf.size  * kTimes;
   stopWatch.PrintResultNoEOL(strHead, kTimes) << "|size=" << sz << std::endl;

   for (int L = 0; L < buf.size; ++L)
      fprintf(stdout, "%02x ", static_cast<fon9::byte>(buf.data[L]));
   fprintf(stdout, "\n");
   if (srec == rec)
      return;
   fprintf(stdout, "\n" "Data not match!\n");
   abort();
}

struct BenchmarkRec {
   int32_t  S32_{-12345};
   uint32_t U32_{12345};
   int64_t  S64_{-1234567890};
   uint64_t U64_{1234567890};

   bool operator==(const BenchmarkRec& r) const {
      return this->S32_ == r.S32_ && this->S64_ == r.S64_
         && this->U32_ == r.U32_ && this->U64_ == r.U64_;
   }
   bool operator!=(const BenchmarkRec& r) const {
      return !this->operator==(r);
   }

   template<typename Archive>
   void serialize(Archive& ar) {
      ar & this->S32_ & this->U32_ & this->S64_ & this->U64_;
   }
};

template <class Archive>
void Serialize(Archive& ar, fon9::ArchiveWorker<Archive,BenchmarkRec>& rec) {
   ar(rec.S32_, rec.U32_, rec.S64_, rec.U64_);
}

//--------------------------------------------------------------------------//

struct BenchmarkRequest {
   uint8_t                  TableId_{3};
   char                     RequestKind_{'N'};
   char                     Market_{'T'};
   char                     Session_{'N'};
   std::string              SesName_{"ApiSes"};
   std::string              UserId_{"fonix"};
   std::string              FromIp_{"127.0.0.1"};
   std::string              UsrDef_{"user-define"};
   fon9::CharAry<4>         BrkNo_{"1234"};
   fon9::CharAry<5>         OrdNo_{"A0001"};
   uint32_t                 IvacNo_{54321};
   std::string              SubacNo_{"subac"};
   char                     IvacNoFlag_{};
   char                     Side_{'B'};
   char                     OType_{'0'};
   fon9::CharAry<6>         Symbol_{"2330"};
   char                     PriType_{'L'};
   double                   Pri_{123.5};
   uint32_t                 Qty_{255};
   fon9::Decimal<int64_t,4> DecPri_{123.5};

   bool operator==(const BenchmarkRequest& r) const {
      return this->TableId_ == r.TableId_
         && this->RequestKind_ == r.RequestKind_
         && this->Market_ == r.Market_
         && this->Session_ == r.Session_
         && this->SesName_ == r.SesName_
         && this->UserId_ == r.UserId_
         && this->FromIp_ == r.FromIp_
         && this->UsrDef_ == r.UsrDef_
         && this->BrkNo_ == r.BrkNo_
         && this->OrdNo_ == r.OrdNo_
         && this->IvacNo_ == r.IvacNo_
         && this->SubacNo_ == r.SubacNo_
         && this->IvacNoFlag_ == r.IvacNoFlag_
         && this->Side_ == r.Side_
         && this->OType_ == r.OType_
         && this->Symbol_ == r.Symbol_
         && this->PriType_ == r.PriType_
         && this->Pri_ == r.Pri_
         && this->DecPri_ == r.DecPri_
         && this->Qty_ == r.Qty_;
   }
   bool operator!=(const BenchmarkRequest& r) const {
      return !this->operator==(r);
   }
   template<typename Archive>
   void serialize(Archive &ar) {
       ar& this->TableId_
         & this->RequestKind_
         & this->Market_
         & this->Session_
         & this->SesName_
         & this->UserId_
         & this->FromIp_
         & this->UsrDef_
         & this->BrkNo_.Chars_
         & this->OrdNo_.Chars_
         & this->IvacNo_
         & this->SubacNo_
         & this->IvacNoFlag_
         & this->Side_
         & this->OType_
         & this->Symbol_.Chars_
         & this->PriType_
         & this->Pri_
         & this->Qty_;
   }
};

template <class Archive>
void Serialize(Archive& ar, fon9::ArchiveWorker<Archive,BenchmarkRequest>& rec) {
      return ar(rec.TableId_,
                rec.RequestKind_,
                rec.Market_,
                rec.Session_,
                rec.SesName_,
                rec.UserId_,
                rec.FromIp_,
                rec.UsrDef_,
                rec.BrkNo_,
                rec.OrdNo_,
                rec.IvacNo_,
                rec.SubacNo_,
                rec.IvacNoFlag_,
                rec.Side_,
                rec.OType_,
                rec.Symbol_,
                rec.PriType_,
                rec.DecPri_,
                rec.Qty_);
}

//--------------------------------------------------------------------------//

template <class Pri, class Qty = uint64_t, size_t BSCount = 5>
struct BenchmarkMarket {
   uint8_t  TableId_{3};
   Pri      BuyPri_[BSCount];
   Qty      BuyQty_[BSCount];
   Pri      SellPri_[BSCount];
   Qty      SellQty_[BSCount];
   BenchmarkMarket() {
      for (size_t L = 0; L < BSCount; ++L) {
         this->BuyQty_[L] = (L + 1) * 100;
         this->SellQty_[L] = (L + 1) * 200;
         this->BuyPri_[L] = this->SellPri_[L] = Pri{123.4};
      }
   }

   bool operator==(const BenchmarkMarket& r) const {
      for (size_t L = 0; L < BSCount; ++L)
         if (this->BuyPri_[L]  != r.BuyPri_[L]
          || this->BuyQty_[L]  != r.BuyQty_[L]
          || this->SellPri_[L] != r.SellPri_[L]
          || this->SellQty_[L] != r.SellQty_[L])
            return false;
      return this->TableId_ == r.TableId_;
   }
   bool operator!=(const BenchmarkMarket& r) const {
      return !this->operator==(r);
   }
   template<typename Archive>
   void serialize(Archive &ar) {
      ar& this->TableId_
        & this->BuyPri_
        & this->BuyQty_
        & this->SellPri_
        & this->SellQty_;
   }

   template <class Archive>
   inline friend void Serialize(Archive& ar, fon9::ArchiveWorker<Archive,BenchmarkMarket>& rec) {
      ar( rec.TableId_
        , rec.BuyPri_
        , rec.BuyQty_
        , rec.SellPri_
        , rec.SellQty_ );
   }
};

//--------------------------------------------------------------------------//

int main(int argc, char *argv[]) {
   fon9::AutoPrintTestInfo utinfo("Benchmark for [yas] & [fon9::Bitv]");

   std::cout << "Codec(0..n):\n";
   constexpr size_t kyasbin = yas::binary | yas::no_header;
   constexpr size_t kyascmp = yas::binary | yas::no_header | yas::compacted;
   BenchmarkSeq<kyasbin>("yas          ");
   BenchmarkSeq<kyascmp>("yas-compacted");
   BitvBenchmarkSeq     ("Bitv         ");
   
   utinfo.PrintSplitter();
   std::cout << "S32,U32,S64,U64:\n";
   BenchmarkRecord<kyasbin>("yas          ", BenchmarkRec{});
   BenchmarkRecord<kyascmp>("yas-compacted", BenchmarkRec{});
   BitvBenchmarkRecord     ("Bitv         ", BenchmarkRec{});

   utinfo.PrintSplitter();
   std::cout << "struct Request:\n";
   BenchmarkRecord<kyasbin>("yas          ", BenchmarkRequest{});
   BenchmarkRecord<kyascmp>("yas-compacted", BenchmarkRequest{});
   BitvBenchmarkRecord     ("Bitv         ", BenchmarkRequest{});

   utinfo.PrintSplitter();
   using YasMarket = ::BenchmarkMarket<double>;
   std::cout << "struct Market:\n";
   BenchmarkRecord<kyasbin>("yas          ", YasMarket{});
   BenchmarkRecord<kyascmp>("yas-compacted", YasMarket{});

   using Fon9Market6 = ::BenchmarkMarket<fon9::Decimal<int64_t,6>>;
   BitvBenchmarkRecord     ("Bitv-Scale6  ", Fon9Market6{});

   using Fon9Market4 = ::BenchmarkMarket<fon9::Decimal<int64_t,4>>;
   BitvBenchmarkRecord     ("Bitv-Scale4  ", Fon9Market4{});
}

//--------------------------------------------------------------------------//
// 測試結果 2018/07/08
// Hardware: HP ProLiant DL380p Gen8 / E5-2680 v2 @ 2.80GHz
// Linux: Ubuntu 16.04.2
// 
// ===== rt-cfg.sh:
// #!/bin/bash
// service irqbalance stop
// # 將 CPU 時脈調到最高.
// max_freq=$(for i in $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies); do echo $i; done | sort -nr | head -1)
// echo $max_freq
// 
// for cpu_max_freq in /sys/devices/system/cpu/cpu*/cpufreq/scaling_max_freq; do echo $max_freq > $cpu_max_freq; done
// for governor in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do echo performance > $governor; done
// 
// #####################################################
// fon9 [Benchmark for [yas] & [fon9::Bitv]] test
// =====================================================
// Codec(0..n):
// yas          : 0.039211076 secs / 10,000,000 times =   3.921107600 ns|size=40,000,000
// yas-compacted: 0.150962338 secs / 10,000,000 times =  15.096233800 ns|size=39,934,080
// Bitv         : 0.094825750 secs / 10,000,000 times =   9.482575000 ns|size=39,934,207
// -----------------------------------------------------
// S32,U32,S64,U64:
// yas          : 0.178419082 secs / 10,000,000 times =  17.841908200 ns|size=240,000,000   // c7 cf ff ff 39 30 00 00 2e fd 69 b6 ff ff ff ff d2 02 96 49 00 00 00 00 
// yas-compacted: 0.540263239 secs / 10,000,000 times =  54.026323900 ns|size=160,000,000   // 82 39 30 02 39 30 84 d2 02 96 49 04 d2 02 96 49 
// Bitv         : 0.369557127 secs / 10,000,000 times =  36.955712700 ns|size=160,000,000   // a1 30 38 91 30 39 a3 49 96 02 d1 93 49 96 02 d2 
// -----------------------------------------------------
// struct Request:
// yas          : 2.417614572 secs / 10,000,000 times = 241.761457200 ns|size=1,390,000,000 // 03 4e 54 4e 06 00 00 00 00 00 00 00 41 70 69 53 65 73 05 00 00 00 00 00 00 00 66 6f 6e 69 78 09 00 00 00 00 00 00 00 31 32 37 2e 30 2e 30 2e 31 0b 00 00 00 00 00 00 00 75 73 65 72 2d 64 65 66 69 6e 65 04 00 00 00 00 00 00 00 31 32 33 34 05 00 00 00 00 00 00 00 41 30 30 30 31 31 d4 00 00 05 00 00 00 00 00 00 00 73 75 62 61 63 00 42 30 06 00 00 00 00 00 00 00 32 33 33 30 00 00 4c 00 00 00 00 00 e0 5e 40 ff 00 00 00 
// yas-compacted: 2.394404408 secs / 10,000,000 times = 239.440440800 ns|size=800,000,000   // 03 4e 54 4e 86 41 70 69 53 65 73 85 66 6f 6e 69 78 89 31 32 37 2e 30 2e 30 2e 31 8b 75 73 65 72 2d 64 65 66 69 6e 65 84 31 32 33 34 85 41 30 30 30 31 02 31 d4 85 73 75 62 61 63 00 42 30 86 32 33 33 30 00 00 4c 00 00 00 00 00 e0 5e 40 01 ff 
// Bitv         : 2.880411845 secs / 10,000,000 times = 288.041184500 ns|size=810,000,000   // 90 03 00 4e 00 54 00 4e 05 41 70 69 53 65 73 04 66 6f 6e 69 78 08 31 32 37 2e 30 2e 30 2e 31 0a 75 73 65 72 2d 64 65 66 69 6e 65 03 31 32 33 34 04 41 30 30 30 31 91 d4 31 04 73 75 62 61 63 f1 00 42 00 30 03 32 33 33 30 00 4c b1 00 04 d3 90 ff 
// -----------------------------------------------------
// struct Market: Price=123.4: yas 使用 double; Bitv 使用 Decimal<Scale 6 or 4>; 因為要計算移除小數位尾端的0; 所以要花些時間.
// yas          : 1.275765271 secs / 10,000,000 times = 127.576527100 ns|size=1,930,000,000 // 03 05 00 00 00 00 00 00 00 9a 99 99 99 99 d9 5e 40 9a 99 99 99 99 d9 5e 40 9a 99 99 99 99 d9 5e 40 9a 99 99 99 99 d9 5e 40 9a 99 99 99 99 d9 5e 40 05 00 00 00 00 00 00 00 64 00 00 00 00 00 00 00 c8 00 00 00 00 00 00 00 2c 01 00 00 00 00 00 00 90 01 00 00 00 00 00 00 f4 01 00 00 00 00 00 00 05 00 00 00 00 00 00 00 9a 99 99 99 99 d9 5e 40 9a 99 99 99 99 d9 5e 40 9a 99 99 99 99 d9 5e 40 9a 99 99 99 99 d9 5e 40 9a 99 99 99 99 d9 5e 40 05 00 00 00 00 00 00 00 c8 00 00 00 00 00 00 00 90 01 00 00 00 00 00 00 58 02 00 00 00 00 00 00 20 03 00 00 00 00 00 00 e8 03 00 00 00 00 00 00 
// yas-compacted: 2.358683589 secs / 10,000,000 times = 235.868358900 ns|size=1,110,000,000 // 03 85 9a 99 99 99 99 d9 5e 40 9a 99 99 99 99 d9 5e 40 9a 99 99 99 99 d9 5e 40 9a 99 99 99 99 d9 5e 40 9a 99 99 99 99 d9 5e 40 85 e4 01 c8 02 2c 01 02 90 01 02 f4 01 85 9a 99 99 99 99 d9 5e 40 9a 99 99 99 99 d9 5e 40 9a 99 99 99 99 d9 5e 40 9a 99 99 99 99 d9 5e 40 9a 99 99 99 99 d9 5e 40 85 01 c8 02 90 01 02 58 02 02 20 03 02 e8 03 
// Bitv-Scale6  : 2.865763074 secs / 10,000,000 times = 286.576307400 ns|size=690,000,000   // 90 03 b1 00 04 d2 b1 00 04 d2 b1 00 04 d2 b1 00 04 d2 b1 00 04 d2 90 64 90 c8 91 01 2c 91 01 90 91 01 f4 b1 00 04 d2 b1 00 04 d2 b1 00 04 d2 b1 00 04 d2 b1 00 04 d2 90 c8 91 01 90 91 02 58 91 03 20 91 03 e8 
// Bitv-Scale4  : 2.716555879 secs / 10,000,000 times = 271.655587900 ns|size=690,000,000   // 90 03 b1 00 04 d2 b1 00 04 d2 b1 00 04 d2 b1 00 04 d2 b1 00 04 d2 90 64 90 c8 91 01 2c 91 01 90 91 01 f4 b1 00 04 d2 b1 00 04 d2 b1 00 04 d2 b1 00 04 d2 b1 00 04 d2 90 c8 91 01 90 91 02 58 91 03 20 91 03 e8 
// =====================================================
// fon9 [Benchmark for [yas] & [fon9::Bitv]] test # END #
// #####################################################
