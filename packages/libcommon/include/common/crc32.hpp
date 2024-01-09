#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace cm {

class CRC32 {
public:
    CRC32() = default;
    ~CRC32() = default;

    void update(const void* data, size_t size);

    void update(std::span<const uint8_t> span)
    {
        update(span.data(), span.size_bytes());
    }

    void update(const std::vector<uint8_t>& vec)
    {
        update(vec.data(), vec.size());
    }

    uint32_t digest() const;

private:
    uint32_t m_state { 0xFFFFFFFF };
};

}