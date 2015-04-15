#ifndef _SNCOMMON_H_
#define _SNCOMMON_H_

#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <stdarg.h>

#include <string>
#include <list>
#include <map>
#include <iostream>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <unordered_map>
using namespace std;

#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_types.hpp>
#include <boost/unordered_map.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/smart_ptr/detail/spinlock.hpp>
#include <boost/thread/tss.hpp>
#include <boost/thread/condition.hpp>

typedef boost::shared_lock<boost::shared_mutex> RLock;
typedef boost::unique_lock<boost::shared_mutex> WLock;

typedef boost::detail::spinlock SpinLock;

struct SNContext;
typedef std::shared_ptr<SNContext> SNContextPtr;
typedef std::weak_ptr<SNContext> SNContextWeakPtr;

#endif
