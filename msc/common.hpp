#pragma once

#include <fmt/core.h>

extern void (*s_write_fn)(const char* str, int length);

template<typename... T>
static inline void println(::fmt::format_string<T...> fmt, T&&... args)
{
    if (!s_write_fn)
        return;

    std::string string = ::fmt::vformat(fmt, ::fmt::make_format_args(args...));
    s_write_fn(string.c_str(), string.size());
}
