#pragma once

#include <common/crc32.hpp>
#include <fmt/format.h>
#include <msc/msc.hpp>

struct VideoStat {
    int freeze_time_ms;
    float frame_rate;

    int width;
    int height;
};

class ReportVideoConsumer : public msc::VideoConsumer {
public:
    ReportVideoConsumer()
        : msc::VideoConsumer()
    {
    }

    ~ReportVideoConsumer() = default;

    VideoStat video_stat()
    {
        int64_t now = msc::rtc_timestamp_ms();

        VideoStat ret {
            .freeze_time_ms = int(now - m_time_last_frame),
            .frame_rate = float(m_frame_count) / float(now - m_time_last_report) * 1000.0f,
            .width = m_width_last_frame,
            .height = m_height_last_frame,
        };

        m_frame_count = 0;
        m_time_last_report = now;
        return ret;
    }

    void reset()
    {
        m_frame_count = 0;
        m_time_last_report = 0;
        m_time_last_frame = 0;
        m_width_last_frame = 0;
        m_height_last_frame = 0;
    }

private:
    void on_video_frame(const msc::VideoFrame& frame) final
    {
        int64_t now = msc::rtc_timestamp_ms();

        m_frame_count++;
        if (m_time_last_report == 0)
            m_time_last_report = now;

        m_time_last_frame = now;
        m_width_last_frame = frame.width;
        m_height_last_frame = frame.height;
    }

    void on_close() final { }

private:
    int64_t m_time_last_report { 0 };
    int64_t m_time_last_frame { 0 };
    int m_width_last_frame { 0 };
    int m_height_last_frame { 0 };

    int m_frame_count { 0 };
};

struct DataStat {
    int freeze_time_ms;
    float frame_rate;
};

class ReportDataConsumer : public msc::DataConsumer {
public:
    ReportDataConsumer(bool validate_data)
        : m_validate_data(validate_data)
    {
    }

    DataStat data_stat()
    {
        int64_t now = msc::rtc_timestamp_ms();

        DataStat ret {
            .freeze_time_ms = int(now - m_time_last_frame),
            .frame_rate = float(m_frame_count) / float(now - m_time_last_report) * 1000.0f,
        };

        m_frame_count = 0;
        m_time_last_report = now;
        return ret;
    }

private:
    void on_data(std::span<const uint8_t> data)
    {
        if (data.size() < 4) {
            fmt::println("[DataConsumer] recv invalid data (too short)");
            return;
        }

        if (m_validate_data) {
            uint32_t checksum = 0;
            std::memcpy(&checksum, data.data(), sizeof(checksum));

            cm::CRC32 crc32;
            crc32.update(data.subspan(4));
            if (crc32.digest() != checksum) {
                fmt::println("[DataConsumer] recv invalid data (checksum failed)");
                return;
            }
        }

        int64_t now = msc::rtc_timestamp_ms();
        m_frame_count++;
        if (m_time_last_report == 0)
            m_time_last_report = now;

        m_time_last_frame = now;
    }

private:
    int64_t m_time_last_report { 0 };
    int64_t m_time_last_frame { 0 };
    int m_frame_count { 0 };
    bool m_validate_data;
};
