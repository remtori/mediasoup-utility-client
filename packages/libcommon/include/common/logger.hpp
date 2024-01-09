#pragma once

#include <iostream>

#include <fmt/core.h>
#include <fmt/chrono.h>
#include <fmt/format.h>

namespace cm
{

namespace detail {

std::ostream* write_stream();

}

void init_logger(const char* filename);

template <typename... T>
void log(fmt::format_string<T...> fmt, T&&... args) {
    auto* write_stream = detail::write_stream();
    if (write_stream) {
        auto now =  std::chrono::system_clock::now();
        auto formatted = fmt::format(fmt, std::forward<T>(args)...);
        *write_stream << fmt::format("[{:%H:%M:%S}] {}\n", now, formatted);
        write_stream->flush();
    }
}

}
