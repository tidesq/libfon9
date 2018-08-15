// \file fon9/Seed_UT.cpp
//
// test: Named/Field/FieldMaker/Seed(Raw)
//
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/seed/Tab.hpp"
#include "fon9/TypeName.hpp"
#include "fon9/TestTools.hpp"

//--------------------------------------------------------------------------//

enum class EnumInt {
   Value0,
   Value1,
};

using EnumHex = fon9::FmtFlag;

enum class EnumChar : char {
   CharA = 'a',
   CharB = 'b',
};

fon9_WARN_DISABLE_PADDING;
struct Data {
   fon9_NON_COPY_NON_MOVE(Data);
   Data() = default;

   const std::string ConstStdStrValue_{"ConstStdStrValue"};
   std::string       StdStrValue_{"StdStrValue"};
   fon9::CharVector  CharVectorValue_{fon9::StrView{"CharVectorValue"}};

   const char        ConstCharValue_{'C'};
   char              CharValue_{'c'};
   const char        ConstChar4Value_[4]{'A', 'B', 'C', 'D'};
   char              Char4Value_[4]{'a', 'b', 'c', 'd'};

   using Char4Ary = fon9::CharAry<4>;
   const Char4Ary    ConstChar4Ary_{"WXYZ"};
   Char4Ary          Char4Ary_{"wxyz"};

   const int         ConstIntValue_{1234};
   unsigned          IntValue_{1234u};
   const EnumInt     ConstEnumIntValue_{EnumInt::Value0};
   EnumInt           EnumIntValue_{EnumInt::Value1};
   const EnumChar    ConstEnumCharValue_{EnumChar::CharA};
   EnumChar          EnumCharValue_{EnumChar::CharB};
   const EnumHex     ConstEnumHexValue_{EnumHex::BaseHEX};
   EnumHex           EnumHexValue_{EnumHex::BaseHex};

   using Byte4Ary = fon9::CharAry<4, fon9::byte>;
   const Byte4Ary    ConstByte4Ary_{"B4AY"};
   Byte4Ary          Byte4Ary_{"b4ay"};
   const fon9::byte  ConstBytes[4]{'B', 'Y', 'T', 'E'};
   fon9::byte        Bytes[4]{'b', 'y', 't', 'e'};
   const fon9::ByteVector  ConstBVec_{fon9::StrView{"BVEC"}};
   fon9::ByteVector        BVec_{fon9::StrView{"bvec"}};

   using Dec = fon9::Decimal<int, 3>;
   const Dec                  ConstDecValue_{12.34};
   Dec                        DecValue_{56.78};
   using Dec0 = fon9::Decimal<int, 3>;
   Dec0                       Dec0Value_{123, 0};
   Dec0                       Dec0Null_{Dec0::Null()};

   const fon9::TimeStamp      ConstTs_{fon9::YYYYMMDDHHMMSS_ToTimeStamp(20180607143758)};
   fon9::TimeStamp            Ts_{fon9::YYYYMMDDHHMMSS_ToTimeStamp(20180607143759)};
   const fon9::TimeInterval   ConstTi_{fon9::TimeInterval_Millisecond(12345)};
   fon9::TimeInterval         Ti_{fon9::TimeInterval_Millisecond(67890)};
};
fon9_WARN_POP;

//--------------------------------------------------------------------------//

class ReqDataRaw : public Data, public fon9::seed::Raw {
   fon9_NON_COPY_NON_MOVE(ReqDataRaw);
public:
   ReqDataRaw() = default;
};

class ReqRawData : public fon9::seed::Raw, public Data {
   fon9_NON_COPY_NON_MOVE(ReqRawData);
public:
   ReqRawData() = default;
};

class ReqData : public Data {
   fon9_NON_COPY_NON_MOVE(ReqData);
public:
   ReqData() = default;
};

//--------------------------------------------------------------------------//

class ReqRawIncData : public fon9::seed::Raw {
   fon9_NON_COPY_NON_MOVE(ReqRawIncData);
public:
   ReqRawIncData() = default;

   Data  Data_;
};

class ReqIncData {
   fon9_NON_COPY_NON_MOVE(ReqIncData);
public:
   ReqIncData() = default;

   Data  Data_;
};

//--------------------------------------------------------------------------//

const char  kFieldSplitter = '|';

template <class ReqT>
std::string TestGetFields(std::ostream* os, const fon9::seed::Tab& tab, const ReqT& req) {
   if (os)
      *os << fon9::TypeName::Make<ReqT>().get() << '\n';

   std::string res;
   fon9::seed::SimpleRawRd rd{fon9::seed::CastToRawPointer(&req)};
   fon9::RevBufferFixedSize<1024> rbuf;
   size_t L = 0;
   while (const fon9::seed::Field* fld = tab.Fields_.Get(L)) {
      rbuf.RewindEOS();
      fld->CellRevPrint(rd, nullptr, rbuf);
      if (os) {
         *os << std::setw(15) << fld->Name_
            << "|ofs=" << std::setw(5) << fld->Offset_
            << "|'" << rbuf.GetCurrent() << "'\n";
      }
      res.append(rbuf.GetCurrent(), rbuf.GetUsedSize() - 1);//-1 for no EOS.
      res.push_back(kFieldSplitter);
      ++L;
   }
   return res;
}

template <class ReqT>
std::string TestGetFields(std::ostream* os, fon9::seed::Fields&& fields) {
   fon9::seed::TabSP tab{new fon9::seed::Tab{fon9::Named{"Req"}, std::move(fields)}};
   ReqT              req;
   return TestGetFields(os, *tab, req);
}

template <class ReqT>
fon9::seed::Fields MakeReqFields() {
   fon9::seed::Fields fields;
   fields.Add(fon9_MakeField(fon9::Named{"ConstStdStr"},   ReqT, ConstStdStrValue_));
   fields.Add(fon9_MakeField(fon9::Named{"StdStr"},        ReqT, StdStrValue_));
   fields.Add(fon9_MakeField(fon9::Named{"CharVector"},    ReqT, CharVectorValue_));
   fields.Add(fon9_MakeField(fon9::Named{"ConstChar"},     ReqT, ConstCharValue_));
   fields.Add(fon9_MakeField(fon9::Named{"Char"},          ReqT, CharValue_));
   fields.Add(fon9_MakeField(fon9::Named{"ConstChar4"},    ReqT, ConstChar4Value_));
   fields.Add(fon9_MakeField(fon9::Named{"Char4"},         ReqT, Char4Value_));
   fields.Add(fon9_MakeField(fon9::Named{"ConstChar4A"},   ReqT, ConstChar4Ary_));
   fields.Add(fon9_MakeField(fon9::Named{"Char4A"},        ReqT, Char4Ary_));
   fields.Add(fon9_MakeField(fon9::Named{"ConstInt"},      ReqT, ConstIntValue_));
   fields.Add(fon9_MakeField(fon9::Named{"Int"},           ReqT, IntValue_));
   fields.Add(fon9_MakeField(fon9::Named{"ConstEnumInt"},  ReqT, ConstEnumIntValue_));
   fields.Add(fon9_MakeField(fon9::Named{"EnumInt"},       ReqT, EnumIntValue_));
   fields.Add(fon9_MakeField(fon9::Named{"ConstEnumChar"}, ReqT, ConstEnumCharValue_));
   fields.Add(fon9_MakeField(fon9::Named{"EnumChar"},      ReqT, EnumCharValue_));
   fields.Add(fon9_MakeField(fon9::Named{"ConstEnumHex"},  ReqT, ConstEnumHexValue_));
   fields.Add(fon9_MakeField(fon9::Named("EnumHex", "FmtFlag", "Test FmtFlag hex output"), ReqT, EnumHexValue_));
   fields.Add(fon9_MakeField(fon9::Named{"ConstByte4Ary"}, ReqT, ConstByte4Ary_));
   fields.Add(fon9_MakeField(fon9::Named{"Byte4Aty"},      ReqT, Byte4Ary_));
   fields.Add(fon9_MakeField(fon9::Named{"ConstBytes"},    ReqT, ConstBytes));
   fields.Add(fon9_MakeField(fon9::Named{"Bytes"},         ReqT, Bytes));
   fields.Add(fon9_MakeField(fon9::Named{"ConstBVec"},     ReqT, ConstBVec_));
   fields.Add(fon9_MakeField(fon9::Named{"BVec"},          ReqT, BVec_));
   fields.Add(fon9_MakeField(fon9::Named{"ConstDec"},      ReqT, ConstDecValue_));
   fields.Add(fon9_MakeField(fon9::Named{"Dec"},           ReqT, DecValue_));
   fields.Add(fon9_MakeField(fon9::Named{"Dec0"},          ReqT, Dec0Value_));
   fields.Add(fon9_MakeField(fon9::Named{"Dec0N"},         ReqT, Dec0Null_));
   fields.Add(fon9_MakeField(fon9::Named{"ConstTs"},       ReqT, ConstTs_));
   fields.Add(fon9_MakeField(fon9::Named{"Ts"},            ReqT, Ts_));
   fields.Add(fon9_MakeField(fon9::Named{"ConstTi"},       ReqT, ConstTi_));
   fields.Add(fon9_MakeField(fon9::Named{"Ti"},            ReqT, Ti_));
   return fields;
}

template <class ReqT>
fon9::seed::Fields MakeReqFieldsIncData() {
   fon9::seed::Fields fields;
   fields.Add(fon9_MakeField(fon9::Named{"ConstStdStr"},   ReqT, Data_.ConstStdStrValue_));
   fields.Add(fon9_MakeField(fon9::Named{"StdStr"},        ReqT, Data_.StdStrValue_));
   fields.Add(fon9_MakeField(fon9::Named{"CharVector"},    ReqT, Data_.CharVectorValue_));
   fields.Add(fon9_MakeField(fon9::Named{"ConstChar"},     ReqT, Data_.ConstCharValue_));
   fields.Add(fon9_MakeField(fon9::Named{"Char"},          ReqT, Data_.CharValue_));
   fields.Add(fon9_MakeField(fon9::Named{"ConstChar4"},    ReqT, Data_.ConstChar4Value_));
   fields.Add(fon9_MakeField(fon9::Named{"Char4"},         ReqT, Data_.Char4Value_));
   fields.Add(fon9_MakeField(fon9::Named{"ConstChar4A"},   ReqT, Data_.ConstChar4Ary_));
   fields.Add(fon9_MakeField(fon9::Named{"Char4A"},        ReqT, Data_.Char4Ary_));
   fields.Add(fon9_MakeField(fon9::Named{"ConstInt"},      ReqT, Data_.ConstIntValue_));
   fields.Add(fon9_MakeField(fon9::Named{"Int"},           ReqT, Data_.IntValue_));
   fields.Add(fon9_MakeField(fon9::Named{"ConstEnumInt"},  ReqT, Data_.ConstEnumIntValue_));
   fields.Add(fon9_MakeField(fon9::Named{"EnumInt"},       ReqT, Data_.EnumIntValue_));
   fields.Add(fon9_MakeField(fon9::Named{"ConstEnumChar"}, ReqT, Data_.ConstEnumCharValue_));
   fields.Add(fon9_MakeField(fon9::Named{"EnumChar"},      ReqT, Data_.EnumCharValue_));
   fields.Add(fon9_MakeField(fon9::Named{"ConstEnumHex"},  ReqT, Data_.ConstEnumHexValue_));
   fields.Add(fon9_MakeField(fon9::Named("EnumHex", "FmtFlag", "Test FmtFlag hex output"), ReqT, Data_.EnumHexValue_));
   fields.Add(fon9_MakeField(fon9::Named{"ConstByte4Ary"}, ReqT, Data_.ConstByte4Ary_));
   fields.Add(fon9_MakeField(fon9::Named{"Byte4Aty"},      ReqT, Data_.Byte4Ary_));
   fields.Add(fon9_MakeField(fon9::Named{"ConstBytes"},    ReqT, Data_.ConstBytes));
   fields.Add(fon9_MakeField(fon9::Named{"Bytes"},         ReqT, Data_.Bytes));
   fields.Add(fon9_MakeField(fon9::Named{"ConstBVec"},     ReqT, Data_.ConstBVec_));
   fields.Add(fon9_MakeField(fon9::Named{"BVec"},          ReqT, Data_.BVec_));
   fields.Add(fon9_MakeField(fon9::Named{"ConstDec"},      ReqT, Data_.ConstDecValue_));
   fields.Add(fon9_MakeField(fon9::Named{"Dec"},           ReqT, Data_.DecValue_));
   fields.Add(fon9_MakeField(fon9::Named{"Dec0"},          ReqT, Data_.Dec0Value_));
   fields.Add(fon9_MakeField(fon9::Named{"Dec0N"},         ReqT, Data_.Dec0Null_));
   fields.Add(fon9_MakeField(fon9::Named{"ConstTs"},       ReqT, Data_.ConstTs_));
   fields.Add(fon9_MakeField(fon9::Named{"Ts"},            ReqT, Data_.Ts_));
   fields.Add(fon9_MakeField(fon9::Named{"ConstTi"},       ReqT, Data_.ConstTi_));
   fields.Add(fon9_MakeField(fon9::Named{"Ti"},            ReqT, Data_.Ti_));
   return fields;
}

//--------------------------------------------------------------------------//

void TestDyRec(const std::string& fldcfg, const std::string& vlist) {
   std::cout << fldcfg << std::endl;
   fon9::StrView      cfg{&fldcfg};
   fon9::seed::Fields fields;
   fon9::seed::MakeFields(cfg, '|', '\n', fields);
   const size_t       fieldsCount = fields.size();
   // 測試重複設定: 檢查 TypeId() 是否正確.
   cfg = fon9::ToStrView(fldcfg);
   fon9_CheckTestResult("MakeFields",
                        fon9::seed::MakeFields(cfg, '|', '\n', fields) == fon9::seed::OpResult::no_error);
   fon9_CheckTestResult("Dup.MakeFields", fieldsCount == fields.size());

   // 測試 DyRec 沒有使用 MakeDyMemRaw() 建立: 必定產生 exception!
   fon9::seed::TabSP tab{new fon9::seed::Tab{fon9::Named{"DyRec"}, std::move(fields)}};
   try {
      ReqRawData req;
      TestGetFields(nullptr, *tab, req);
      fon9_CheckTestResult("DyRec", false);
   }
   catch (const std::exception& e) {
      fon9_CheckTestResult(e.what(), true);
   }

   // 測試 DyRec 設定值 StrToCell().
   struct DyRaw : public fon9::seed::Raw {
      fon9_NON_COPY_NON_MOVE(DyRaw);
      DyRaw() = default;
   };
   auto dyreq = std::unique_ptr<DyRaw>(fon9::seed::MakeDyMemRaw<DyRaw>(*tab));
   cfg = fon9::ToStrView(vlist);
   fon9::seed::SimpleRawWr wr{fon9::seed::CastToRawPointer(dyreq.get())};
   for (size_t idx = 0;; ++idx) {
      const fon9::seed::Field* wrfld = tab->Fields_.Get(idx);
      if (!wrfld)
         break;
      wrfld->StrToCell(wr, fon9::StrFetchNoTrim(cfg, kFieldSplitter));
   }
   fon9_CheckTestResult("DyRec.StrToCell", vlist == TestGetFields(nullptr, *tab, *dyreq));
}

//--------------------------------------------------------------------------//

void TestDeserializeNamed(fon9::StrView src, char chSpl, int chTail) {
   std::cout << "\n" << src.begin() << " => ";
   fon9::Named n1 = fon9::DeserializeNamed(src, chSpl, chTail);
   std::cout << "name=" << n1.Name_ << "|title=" << n1.GetTitle() << "|desc=" << n1.GetDescription() << std::endl;

   fon9_CheckTestResult("DeserializeNamed",
      (n1.Name_ == "fonwin" && n1.GetTitle() == "Fon9 title" && n1.GetDescription() == "Fon9 Description"));
   if (chTail == -1)
      fon9_CheckTestResult("DeserializeNamed:src.begin()==src.end()", (src.begin() == src.end()));
   else
      fon9_CheckTestResult("DeserializeNamed:src.begin()[-1]==chTail", (*(src.begin() - 1) == chTail));

   std::string s1;
   fon9::SerializeNamed(s1, n1, chSpl, chTail);
   src = fon9::ToStrView(s1);
   fon9::Named n2 = fon9::DeserializeNamed(src, chSpl, chTail);
   fon9_CheckTestResult(("SerializeNamed()=" + s1).c_str(),
      (n1.Name_ == n2.Name_ && n1.GetTitle() == n2.GetTitle() && n1.GetDescription() == n2.GetDescription()));
}
void TestDeserializeNamed() {
   TestDeserializeNamed("fonwin|Fon9 title|Fon9 Description", '|', -1);
   TestDeserializeNamed("fonwin|Fon9 title|Fon9 Description\t", '|', '\t');
   TestDeserializeNamed("  fonwin  |   Fon9 title  |  Fon9 Description   ", '|', -1);
   TestDeserializeNamed("  fonwin  |   Fon9 title  |  Fon9 Description   \t" "test tail message", '|', '\t');
   TestDeserializeNamed("  fonwin  |  'Fon9 title' |  \"Fon9 Description\"   ", '|', -1);
   TestDeserializeNamed("  fonwin  |  'Fon9 title' |  \"Fon9 Description\"   \t" "test tail message", '|', '\t');
}

//--------------------------------------------------------------------------//

int main(int argc, char** args) {
   (void)argc; (void)args;
#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
   //_CrtSetBreakAlloc(176);
#endif
   fon9::AutoPrintTestInfo utinfo{"Seed/FieldMaker"};

   TestDeserializeNamed();

   utinfo.PrintSplitter();
   std::string vlist = TestGetFields<ReqRawData>(&std::cout, MakeReqFields<ReqRawData>());
   fon9_CheckTestResult("ReqDataRaw",    vlist == TestGetFields<ReqDataRaw>(nullptr, MakeReqFields<ReqDataRaw>()));
   fon9_CheckTestResult("ReqData",       vlist == TestGetFields<ReqData>(nullptr, MakeReqFields<ReqData>()));
   fon9_CheckTestResult("ReqRawIncData", vlist == TestGetFields<ReqRawData>(nullptr, MakeReqFieldsIncData<ReqRawIncData>()));
   fon9_CheckTestResult("ReqIncData",    vlist == TestGetFields<ReqData>(nullptr, MakeReqFieldsIncData<ReqIncData>()));

   TestDyRec(MakeFieldsConfig(MakeReqFields<ReqRawData>(), '|', '\n'), vlist);
}
