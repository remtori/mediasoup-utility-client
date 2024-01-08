#include "./media_sink.hpp"

namespace msc {

void AudioSinkImpl::OnData(
    const void* data,
    int bits_per_sample,
    int sample_rate,
    size_t number_of_channels,
    size_t number_of_frames,
    absl::optional<int64_t> absolute_capture_timestamp_ms)
{
    AudioData audio_data {
        .timestamp_ms = absolute_capture_timestamp_ms.value_or(rtc::TimeMillis()),
        .bits_per_sample = bits_per_sample,
        .sample_rate = sample_rate,
        .number_of_channels = static_cast<int>(number_of_channels),
        .number_of_frames = static_cast<int>(number_of_frames),
        .data = data,
    };

    m_user_consumer->on_audio_data(audio_data);
}

void VideoSinkImpl::OnFrame(const webrtc::VideoFrame& frame)
{
    // auto i420_buffer = frame.video_frame_buffer()->ToI420();

    VideoFrame video_frame = VideoFrame {
        .timestamp_ms = frame.timestamp(),
        .width = frame.width(),
        .height = frame.height(),
        //        .data_y = i420_buffer->DataY(),
        //        .data_u = i420_buffer->DataU(),
        //        .data_v = i420_buffer->DataV(),
        //        .stride_y = i420_buffer->StrideY(),
        //        .stride_u = i420_buffer->StrideU(),
        //        .stride_v = i420_buffer->StrideV(),
    };

    m_user_consumer->on_video_frame(video_frame);
}

}