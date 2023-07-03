#pragma once

#ifdef NDEBUG
#    define DID_UNDEF_NDEBUG
#    undef NDEBUG
#endif

#include <cassert>
#include <thread>

#define ASSERT_SAME_THREAD(thread_id) assert(thread_id == std::this_thread::get_id())

#ifdef DID_UNDEF_NDEBUG
#    undef DID_UNDEF_NDEBUG
#    define NDEBUG
#endif
