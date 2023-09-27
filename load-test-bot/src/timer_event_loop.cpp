#include "timer_event_loop.hpp"

#include <hv/EventLoopThread.h>

hv::EventLoop& timer_event_loop() {
    static hv::EventLoopThread s_timer_event_loop_thread {};
    if (!s_timer_event_loop_thread.isRunning())
        s_timer_event_loop_thread.start();

    return *s_timer_event_loop_thread.loop();
}
