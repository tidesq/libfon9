/// \file fon9/FileRevRead_UT.cpp
/// \author fonwinz@gmail.com
#include "fon9/FileRevRead.hpp"
#include "fon9/RevPrint.hpp"

//--------------------------------------------------------------------------//

bool OpenFile(const char* info, fon9::File& fd, const char* fname, fon9::FileMode fm) {
   printf("%s %s\n", info, fname);
   auto res = fd.Open(fname, fm);
   if (res)
      return true;
   puts(fon9::RevPrintTo<std::string>("Open error: ", res).c_str());
   return false;
}

//--------------------------------------------------------------------------//

int main(int argc, char** args) {
   #if defined(_MSC_VER) && defined(_DEBUG)
      _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
      //_CrtSetBreakAlloc(176);
   #endif
   if(argc < 3) {
      printf("Reverse InputFile lines to OutputFile.\n"
             "Usage:  InputFile OutputFile\n");
      return 3;
   }
   fon9::File fdin;
   if (!OpenFile("Input  file: ", fdin, args[1], fon9::FileMode::Read))
      return 3;
   fon9_MSC_WARN_DISABLE_NO_PUSH(4820 4355);
   struct RevReader : public fon9::RevReadSearcher<fon9::FileRevReadBuffer<1024*4>, fon9::FileRevSearch> {
      fon9_NON_COPY_NON_MOVE(RevReader);
      // using base = fon9::RevReadSearcher<fon9::FileRevReadBuffer<1024 * 4>, fon9::FileRevSearch>;
      RevReader() = default;

      unsigned long  LineCount_{0};
      fon9::File     FdOut_;

      virtual fon9::LoopControl OnFileBlock(size_t rdsz) override {
         if (this->RevSearchBlock(this->GetBlockPos(), '\n', rdsz) == fon9::LoopControl::Break)
            return fon9::LoopControl::Break;
         if (this->GetBlockPos() == 0 && this->LastRemainSize_ > 0)
            this->AppendLine(this->BlockBuffer_, this->LastRemainSize_);
         return fon9::LoopControl::Continue;
      }
      virtual fon9::LoopControl OnFoundChar(char* pbeg, char* pend) override {
         ++pbeg; // *pbeg=='\n'; => 應放在行尾.
         if (pbeg == pend && this->LineCount_ == 0)
            ++this->LineCount_;
         else
            this->AppendLine(pbeg, static_cast<size_t>(pend - pbeg));
         return fon9::LoopControl::Continue;
      }
      void AppendLine(char* pbeg, size_t lnsz) {
         this->FdOut_.Append(pbeg, lnsz);
         this->FdOut_.Append("\n", 1);
         ++this->LineCount_;
      }
   };
   RevReader reader;
   if (!OpenFile("Output file: ", reader.FdOut_, args[2], fon9::FileMode::Append | fon9::FileMode::CreatePath | fon9::FileMode::Trunc))
      return 3;
   auto res = reader.Start(fdin);
   printf("Line count: %lu\n", reader.LineCount_);
   if (!res)
      puts(fon9::RevPrintTo<std::string>("Error: ", res).c_str());
   return 0;
}
