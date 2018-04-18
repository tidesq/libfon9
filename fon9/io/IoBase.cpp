/// \file fon9/io/IoBase.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/IoBase.hpp"
#include "fon9/Utility.hpp"

namespace fon9 {
template <bool isItemCorrect, size_t arysz>
constexpr StrView MakeEnumStrView(const char(&str)[arysz]) {
   return StrView{str};
   static_assert(isItemCorrect, "Incorrect enum item#");
}
/// \ingroup AlNum
/// 協助建立enum列舉字串.
/// - enum EnumT { item, item1, item2 };
/// - fon9_MAKE_ENUM_StrView(0, EnumT, item) 會建立 "0.item"
/// - 並檢查 item 是否正確(此例為: item==0)
/// - 通常用在建立 enum 字串陣列:
///   \code
///      const StrView EnumT_StrView[] {
///         fon9_MAKE_ENUM_StrView(0, EnumT, item), // StrView{"0.item"}
///         fon9_MAKE_ENUM_StrView(1, EnumT, item1),// StrView{"1.item1"}
///         fon9_MAKE_ENUM_StrView(2, EnumT, item2),// StrView{"2.item2"}
///       //fon9_MAKE_ENUM_StrView(4, EnumT, item), // compile error! item!=4
///      };
///   \endcode
#define fon9_MAKE_ENUM_StrView(n,EnumClass,item) \
      fon9::MakeEnumStrView<n==static_cast<std::underlying_type<EnumClass>::type>(item)>(#n "." #item)
#define fon9_MAKE_ENUM_CLASS_StrView(n,EnumClass,item) \
      fon9::MakeEnumStrView<n==static_cast<std::underlying_type<EnumClass>::type>(EnumClass::item)>(#n "." #item)
}

namespace fon9 { namespace io {

// iostStrMap[] 應該只有資料, 不應該有用來初始化的程式碼!
// g++ 4.8.4 查看 IoBase.s 是否有最佳化:
//    g++ -S fon9/io/IoBase.cpp -o IoBase.s -I ~/devel/fon9 -O2 -std=c++11
// VC 19.00.23506.0: 查看 IoBase.cod 是否有最佳化:
//    cl fon9/io/IoBase.cpp /O2 /FAcs /I \devel\fon9 /D fon9_EXPORT /c
static const StrView iostStrMap[]{
   fon9_MAKE_ENUM_CLASS_StrView( 0, State, Initializing),//"0.Initializing"
   fon9_MAKE_ENUM_CLASS_StrView( 1, State, Initialized),
   fon9_MAKE_ENUM_CLASS_StrView( 2, State, OpenConfigError),
   fon9_MAKE_ENUM_CLASS_StrView( 3, State, Opening),
   fon9_MAKE_ENUM_CLASS_StrView( 4, State, WaittingLinkIn),
   fon9_MAKE_ENUM_CLASS_StrView( 5, State, Linking),
   fon9_MAKE_ENUM_CLASS_StrView( 6, State, LinkError),
   fon9_MAKE_ENUM_CLASS_StrView( 7, State, LinkReady),
   fon9_MAKE_ENUM_CLASS_StrView( 8, State, LinkBroken),
   fon9_MAKE_ENUM_CLASS_StrView( 9, State, Listening),
   fon9_MAKE_ENUM_CLASS_StrView(10, State, ListenBroken),
   fon9_MAKE_ENUM_CLASS_StrView(11, State, Lingering),
   fon9_MAKE_ENUM_CLASS_StrView(12, State, Closing),
   fon9_MAKE_ENUM_CLASS_StrView(13, State, Closed),
   fon9_MAKE_ENUM_CLASS_StrView(14, State, Disposing),
   fon9_MAKE_ENUM_CLASS_StrView(15, State, Destructing),
   {"?"}
};
fon9_API StrView GetStateStr(State st) {
   size_t ust = static_cast<size_t>(static_cast<std::underlying_type<State>::type>(st));
   if (ust >= numofele(iostStrMap))
      ust = numofele(iostStrMap) - 1;
   return iostStrMap[ust];
}

} } // namespaces
