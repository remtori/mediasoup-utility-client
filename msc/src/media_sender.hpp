#pragma once

#include "msc/msc.hpp"
#include <api/video/i420_buffer.h>
#include <media/base/adapted_video_track_source.h>
#include <media/base/audio_source.h>
#include <mediasoupclient.hpp>
#include <pc/local_audio_source.h>
#include <rtc_base/helpers.h>
#include <third_party/libyuv/include/libyuv.h>

namespace msc {

class VideoSenderImpl : public VideoSender
    , public rtc::AdaptedVideoTrackSource {
public:
    VideoSenderImpl(
        int required_alignment,
        bool is_screen_cast)
        : rtc::AdaptedVideoTrackSource(required_alignment)
        , m_is_screen_cast(is_screen_cast)
    {
    }

    ~VideoSenderImpl() override
    {
        set_state(SourceState::kEnded);
    }

    bool remote() const override { return false; }
    bool is_screencast() const override { return m_is_screen_cast; }
    absl::optional<bool> needs_denoising() const override { return false; }

    SourceState state() const override { return m_state; }
    void set_state(SourceState new_state)
    {
        if (m_state != new_state) {
            std::lock_guard lk(m_mutex);

            m_state = new_state;
            FireOnChanged();
        }
    }

    bool is_closed() override { return m_state != SourceState::kEnded; }

    std::optional<MutableVideoFrame> adapt_frame(
        int sourceWidth, int sourceHeight, int64_t timestamp,
        int* adapted_width, int* adapted_height,
        int* crop_width, int* crop_height,
        int* crop_x, int* crop_y) override
    {
        bool ret = rtc::AdaptedVideoTrackSource::AdaptFrame(sourceWidth, sourceHeight, timestamp, adapted_width, adapted_height, crop_width, crop_height, crop_x, crop_y);
        if (!ret)
            return {};

        if (m_buffer != nullptr || m_buffer->width() != *adapted_width || m_buffer->height() != *adapted_height) {
            m_buffer = webrtc::I420Buffer::Create(*adapted_width, *adapted_height);
        }

        return MutableVideoFrame {
            .timestamp_ms = 0,
            .width = m_buffer->width(),
            .height = m_buffer->height(),
            .data_y = m_buffer->MutableDataY(),
            .data_u = m_buffer->MutableDataU(),
            .data_v = m_buffer->MutableDataV(),
            .stride_y = m_buffer->StrideY(),
            .stride_u = m_buffer->StrideU(),
            .stride_v = m_buffer->StrideV(),
        };
    }

    void send_video_frame(MutableVideoFrame video_frame) override
    {
        rtc::AdaptedVideoTrackSource::OnFrame(webrtc::VideoFrame::Builder()
                                                  .set_video_frame_buffer(m_buffer)
                                                  .set_rotation(webrtc::kVideoRotation_0)
                                                  .set_timestamp_us(video_frame.timestamp_ms * 1000)
                                                  .build());
    }

private:
    bool m_is_screen_cast;

    std::recursive_mutex m_mutex {};
    SourceState m_state { kInitializing };
    rtc::scoped_refptr<webrtc::I420Buffer> m_buffer { nullptr };
};

class AudioSenderImpl : public AudioSender {
public:
    AudioSenderImpl()
    {
    }

    ~AudioSenderImpl()
    {
        if (m_producer) {
            m_producer->Close();
        }
    }

    void init(std::unique_ptr<mediasoupclient::Producer> producer, rtc::scoped_refptr<webrtc::AudioTrackInterface> track)
    {
        m_producer = std::move(producer);
        m_track = std::move(track);
        m_source = m_track->GetSource();
    }

private:
    bool is_closed() override
    {
        return false;
    }

    void send_audio_data(const MutableAudioData& data) override
    {
        std::lock_guard lk(m_mutex);

        for (auto& sink : m_sinks) {
            sink->OnData(data.data, data.bits_per_sample, data.sample_rate, data.number_of_channels, data.number_of_frames, data.timestamp_ms);
        }
    }

private:
    std::mutex m_mutex {};
    std::vector<webrtc::AudioTrackSinkInterface*> m_sinks {};
    std::unique_ptr<mediasoupclient::Producer> m_producer {};
    rtc::scoped_refptr<webrtc::AudioSourceInterface> m_source {};
    rtc::scoped_refptr<webrtc::AudioTrackInterface> m_track {};
};

class DataSenderImpl : public DataSender {
public:
    DataSenderImpl(std::unique_ptr<mediasoupclient::DataProducer> producer)
        : m_producer(std::move(producer))
    {
    }

    ~DataSenderImpl()
    {
        m_producer->Close();
    }

private:
    bool is_closed() override
    {
        return m_producer->GetReadyState() != webrtc::DataChannelInterface::kOpen;
    }

    uint64_t buffered_amount() override
    {
        return m_producer->GetBufferedAmount();
    }

    void send_data(std::span<const uint8_t> data) override
    {
        std::lock_guard lk(m_mutex);
        if (is_closed()) {
            return;
        }

        m_buffer.EnsureCapacity(data.size_bytes());
        std::memcpy(m_buffer.MutableData(), data.data(), data.size_bytes());

        webrtc::DataBuffer data_buffer(m_buffer, true);
        m_producer->Send(data_buffer);
    }

private:
    std::mutex m_mutex {};
    rtc::CopyOnWriteBuffer m_buffer {};
    std::unique_ptr<mediasoupclient::DataProducer> m_producer;
};

}
