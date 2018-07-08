/// \file fon9/HostId.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_HostId_hpp__
#define __fon9_HostId_hpp__
#include "fon9/sys/Config.hpp"
#include <stdint.h>

namespace fon9 {

using HostId = uint32_t;

/// \ingroup Misc
/// 需在啟動時由使用者設定內容, 在沒設定之前為 LocalHostId_==0.
extern fon9_API HostId    LocalHostId_;

} // namespaces
#endif//__fon9_HostId_hpp__
