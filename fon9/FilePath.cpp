/// \file fon9/FilePath.cpp
/// \author fonwinz@gmail.com
#include "fon9/FilePath.hpp"
#include "fon9/StrTools.hpp"
#include <algorithm>

#ifdef fon9_WINDOWS
/// returns zero on success, or - 1 if an error occurred.
inline static int mkdir(const char *pathname, fon9::FilePath::mode_t /*mode*/) {
   if (CreateDirectory(pathname, nullptr))
      return 0;
   return -1;
}
#endif

namespace fon9 {

/// retval.first = PathNameEndOffset;  retval.second = FileNameBeginOffset;
using FindSplResult = std::pair<unsigned, unsigned>;

/// 先找最後一個 '/', 在 Windows 如果找不到, 則看看 fname[1]==':' ?
static FindSplResult FindLastSpl(const std::string& fname) {
   std::string::size_type pos = fname.rfind('/');
   if (pos == std::string::npos) {
   #ifdef fon9_WINDOWS
      if (fname.size() > 2 && fname[1] == ':')
         return FindSplResult{2,2};
   #endif
      return FindSplResult{0,0};
   }
   return FindSplResult{static_cast<int>(pos), static_cast<int>(pos) + 1};
}

std::string FilePath::AppendPathTail(StrView path) {
   if (path.empty())
      return "./";
   std::string res{NormalizePathName(path)};
   auto        tailRemoved = RemovePathTail(&res);
   if (res.size() == tailRemoved.size())
      res.push_back('/');
   else
      res.erase(tailRemoved.size() + 1);
   return res;
}

StrView FilePath::RemovePathTail(StrView path) {
   if (path.empty())
      return StrView{"."};
   const char*       end = path.end();
   const char* const begin = path.begin();
   if (begin == end)
      return path;
   while (*--end == '/') {
      if (begin == end)
         return StrView{begin, begin};
   }
   return StrView{begin, end + 1};
}

StrView FilePath::RemovePathHead(StrView path) {
   while (path.Get1st() == '/')
      path.SetBegin(path.begin() + 1);
   return path;
}


std::string FilePath::NormalizePathName(StrView fname) {
   std::string  result = fname.ToString();
   std::replace(result.begin(), result.end(), '\\', '/');
   return result;
}

std::string FilePath::ExtractPathName(StrView fname) {
   std::string    name = NormalizePathName(fname);
   FindSplResult  spl = FindLastSpl(name);
   return name.substr(0, spl.first);
}

std::string FilePath::ExtractFileName(StrView fname) {
   std::string    name = NormalizePathName(fname);
   FindSplResult  spl = FindLastSpl(name);
   return name.substr(spl.second);
}

void FilePath::SplitPathFileName(StrView fname, std::string& path, std::string& name) {
   std::string    fn = NormalizePathName(fname);
   FindSplResult  spl = FindLastSpl(fn);
   path = fn.substr(0, spl.first);
   name = fn.substr(spl.second);
}

FilePath::Result FilePath::MakePathTree(StrView pathName, mode_t pathAccessMode) {
   if (pathName.empty())
      return Result{0};
   std::string name = NormalizePathName(pathName);
   if (name.back() == '/') {
      name.resize(name.size() - 1);
      if (name.empty())
         return Result{0};
   }
   char* const  ppath = &*name.begin();
   char* ppos = ppath + (*ppath == '/');
   for (;;) {
      if ((ppos = strchr(ppos, '/')) != nullptr)
         *ppos = 0;
      #ifdef fon9_WINDOWS
         if (ppath[1] == ':') {
            switch (ppath[2]) {
            case '/': // "x:/"
               if (ppath[3] != 0)
                  break;
               // no break; same as "x:"
            case 0: // "x:"
               goto __NextSpl;
            }
         }
      #endif
      struct stat fst;
      if (stat(ppath, &fst) == 0) {
         if (!S_ISDIR(fst.st_mode)) // 已存在的不是路徑.
            return std::make_error_condition(std::errc::not_a_directory);
      }
      else {
         if (mkdir(ppath, pathAccessMode) != 0)
            return GetSysErrC();
      }
      #ifdef fon9_WINDOWS
__NextSpl:
      #endif
      if (ppos == nullptr)
         return Result{0};
      *ppos++ = '/';
   }
}

std::string FilePath::GetFullName(StrView fname) {
   std::string fpath = fname.ToString();
   char        fnbuf[1024 * 64];
#ifdef fon9_WINDOWS
   if (_fullpath(fnbuf, fpath.c_str(), sizeof(fnbuf)))
      return std::string(fnbuf);
#else
   if (realpath(fpath.c_str(), fnbuf))
      return std::string(fnbuf);
#endif
   return fpath;
}

std::string FilePath::MergePath(StrView curPath, StrView newPath) {
   std::string result;
   if (newPath.empty())
      result = curPath.ToString();
   else {
      switch (newPath.Get1st()) {
      case '/': // newPath 開頭是 "/" => 使用絕對路徑.
         result = newPath.ToString();
         break;
      default: // 加在現有路徑之後.
         result = curPath.ToString();
         if (!result.empty() && result.back() != '/')
            result.push_back('/');
         newPath.AppendTo(result);
         break;
      }
   }
   while (result.size() > 1 && result.back() == '/')
      result.pop_back();
   return result;
}

std::string FilePath::AppendPath(StrView curPath, StrView appPath) {
   const char* pbeg = appPath.begin();
   const char* pend = appPath.end();
   while (pbeg != pend) {
      switch (*pbeg) {
      case '/':
      case '\\':
         ++pbeg;
         continue;
      }
      return MergePath(curPath, StrView{pbeg, pend});
   }
   return MergePath(curPath, StrView{});
}

bool FilePath::HasPrefixPath(StrView fname) {
   switch (fname.Get1st()) {
   case '.':
      if (fname.size() >= 2)
         switch (fname.begin()[1]) {
         case '/': // fname = "./filename";
            return true;
         case '.':
            if (fname.size() >= 3 && fname.begin()[2] == '/') // fname = "../filename";
               return true;
            break;
         }
      break;
   case '/': // fname = "/filename";
      return true;
   }
   return false;
}

FilePath::StrList FilePath::SplitPathList(StrView& fname) {
   StrList plist;
   for (;;) {
      // 移除前端的 "./" or "/"
      // 如果前端是 "../" 則回到上一層: plist.pop_back()
      for (;;) {
         switch (fname.Get1st()) {
         case '/':
            fname.SetBegin(fname.begin() + 1);
            continue;
         case '.':
            size_t szfname = fname.size();
            if (szfname == 1) { // 移除 fname 尾端最後一個「.」
               fname.SetBegin(fname.begin() + 1);
               break;
            }
            switch (fname.begin()[1]) {
            case '/': // 移除 "./"
               fname.SetBegin(fname.begin() + 2);
               continue;
            case '.': // 移除 "../" or 尾端最後的「..」
               if (szfname == 2 || (szfname >= 3 && fname.begin()[2] == '/')) {
                  if (plist.empty()) // error: 沒有前一層路徑!
                     return StrList{};
                  plist.pop_back();
                  fname.SetBegin(fname.begin() + (szfname == 2 ? 2 : 3));
                  continue;
               }
               break;
            }
            break;
         }
         break;
      }
      StrView item{SbrFetchField(fname, '/', StrBrArg::Quotation_)};
      if (item.empty())
         break;
      plist.push_back(item);
   }
   return plist;
}

std::string FilePath::MergePathList(const StrList& plist) {
   if (plist.empty())
      return std::string{};
   std::string res = plist[0].ToString();
   auto iend = plist.end();
   auto i = plist.begin();
   while (++i != iend) {
      res.push_back('/');
      i->AppendTo(res);
   }
   return res;
}

} // namespaces
