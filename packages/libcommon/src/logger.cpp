#include "common/logger.hpp"

#include <filesystem>
#include <fstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace cm
{

static std::ostream* s_output_stream;

namespace detail {

std::ostream* write_stream()
{
    return s_output_stream;
}

}

void init_logger(const char* filename)
{
    if (s_output_stream != nullptr)
        return;

    if (filename == nullptr) {
        s_output_stream = &std::cout;
        return;
    }

    std::string bin_path_string;
#ifdef _WIN32
    bin_path_string.resize(255, 0);
    GetModuleFileName(NULL, bin_path_string.data(), bin_path_string.size());
#else
    bin_path_string = std::filesystem::canonical("/proc/self/exe").string();
#endif

    std::filesystem::path bin_path(bin_path_string);
    auto log_path = (bin_path.parent_path() / filename).string();

    s_output_stream = new std::ofstream(log_path.c_str());
}

}