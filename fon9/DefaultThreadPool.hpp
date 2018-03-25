/// \file fon9/DefaultThreadPool.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_DefaultThreadPool_hpp__
#define __fon9_DefaultThreadPool_hpp__
#include "fon9/MessageQueue.hpp"

namespace fon9 {

/// \ingroup Thrs
/// 在 GetDefaultThreadPool() 裡面執行的單一作業。
using DefaultThreadTask = std::function<void()>;
struct DefaultThreadTaskHandler;
using DefaultThreadPool = MessageQueueService<DefaultThreadTaskHandler, DefaultThreadTask>;

/// \ingroup Thrs
/// 取得 fon9 提供的一個 thread pool.
/// * 一般用於不急迫, 但比較花時間的簡單工作, 例如: 寫檔、domain name 查找...
/// * 程式結束時, 剩餘的工作會被拋棄!
fon9_API DefaultThreadPool& GetDefaultThreadPool();

} // namespaces
#endif//__fon9_DefaultThreadPool_hpp__
