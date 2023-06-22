#pragma once

#include "./lib.h"
#include <mediasoupclient.hpp>

namespace impl {

class BaseSink {
public:
    virtual ~BaseSink() {};

    virtual bool is_audio_sink() const = 0;
    virtual mediasoupclient::Consumer* consumer() const = 0;
};

class AudioSink : public BaseSink
    , public webrtc::AudioTrackSinkInterface {
public:
    AudioSink(mediasoupclient::Consumer* consumer, void* user_ctx, OnAudioData on_audio_data)
        : m_consumer(consumer)
        , m_user_ctx(user_ctx)
        , m_on_audio_data(on_audio_data)
    {
    }

    bool is_audio_sink() const override { return true; }
    mediasoupclient::Consumer* consumer() const override { return m_consumer; }

    void OnData(
        const void* data,
        int bits_per_sample,
        int sample_rate,
        size_t number_of_channels,
        size_t number_of_frames,
        absl::optional<int64_t> absolute_capture_timestamp_ms) override;

private:
    mediasoupclient::Consumer* m_consumer;
    void* m_user_ctx;
    OnAudioData m_on_audio_data;
};

class VideoSink : public BaseSink
    , public rtc::VideoSinkInterface<webrtc::VideoFrame> {
public:
    VideoSink(mediasoupclient::Consumer* consumer, void* user_ctx, OnVideoFrame on_video_data)
        : m_consumer(consumer)
        , m_user_ctx(user_ctx)
        , m_on_video_frame(on_video_data)
    {
    }

    bool is_audio_sink() const override { return false; }
    mediasoupclient::Consumer* consumer() const { return m_consumer; }

    void OnFrame(const webrtc::VideoFrame& frame) override;

private:
    mediasoupclient::Consumer* m_consumer;
    void* m_user_ctx;
    OnVideoFrame m_on_video_frame;
    std::vector<uint8_t> m_data {};
};

}