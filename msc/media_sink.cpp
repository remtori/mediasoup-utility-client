#include "./media_sink.hpp"

#include <libyuv.h>

namespace impl {

void AudioSink::OnData(
    const void* data,
    int bits_per_sample,
    int sample_rate,
    size_t number_of_channels,
    size_t number_of_frames,
    absl::optional<int64_t> absolute_capture_timestamp_ms)
{
    if (!m_on_audio_data)
        return;

    ::MscAudioData audio_data {
        .data = data,
        .bits_per_sample = bits_per_sample,
        .sample_rate = sample_rate,
        .number_of_channels = static_cast<int>(number_of_channels),
        .number_of_frames = static_cast<int>(number_of_frames),
        .absolute_capture_timestamp_ms = absolute_capture_timestamp_ms.value_or(rtc::TimeMillis()),
    };

    m_on_audio_data(m_user_ctx, audio_data);
}

void VideoSink::OnFrame(const webrtc::VideoFrame& frame)
{
    if (!m_on_video_frame)
        return;

    m_data.resize(4 * frame.width() * frame.height());

    auto i420_buffer = frame.video_frame_buffer()->ToI420();
    libyuv::ConvertFromI420(
        i420_buffer->DataY(), i420_buffer->StrideY(), i420_buffer->DataU(),
        i420_buffer->StrideU(), i420_buffer->DataV(), i420_buffer->StrideV(),
        m_data.data(), 0, frame.width(), frame.height(),
        libyuv::FOURCC_ABGR);

    ::MscVideoFrame video_frame {
        .data = m_data.data(),
        .width = frame.width(),
        .height = frame.height(),
        .timestamp = frame.timestamp()
    };

    m_on_video_frame(m_user_ctx, video_frame);
}

}