#pragma once

#include <hv/EventLoopThread.h>

hv::EventLoopThread& timer_event_loop_thread();

inline hv::EventLoop& timer_event_loop()
{
    return *timer_event_loop_thread().loop();
}
