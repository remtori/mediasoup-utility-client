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

class AudioSenderImpl : public AudioSender,
                        public webrtc::LocalAudioSource
{
public:
    void AddSink(webrtc::AudioTrackSinkInterface* newSink) override
    {
        std::lock_guard lk(m_mutex);
        for (auto* sink : m_sinks)
            if (sink == newSink) return;

        m_sinks.push_back(newSink);
    }

    void RemoveSink(webrtc::AudioTrackSinkInterface* sink) override
    {
        std::lock_guard lk(m_mutex);
        (void) std::remove(m_sinks.begin(), m_sinks.end(), sink);
    }
private:
    std::mutex m_mutex {};
    std::vector<webrtc::AudioTrackSinkInterface*> m_sinks {};
};

}
