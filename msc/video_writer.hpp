#pragma once

#include "./media_sink.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace impl {

using WriteFn = int(void* opaque, uint8_t* buffer, int buffer_size);

class VideoWriter : public BaseSink
    , public rtc::VideoSinkInterface<webrtc::VideoFrame> {
public:
    VideoWriter(mediasoupclient::Consumer* consumer, void* user_ctx, WriteFn);
    ~VideoWriter();

    bool is_audio_sink() const override { return false; }
    mediasoupclient::Consumer* consumer() const { return m_consumer; }

    void OnFrame(const webrtc::VideoFrame& frame) override;

private:
    void setup(int fps, int width, int height);

private:
    mediasoupclient::Consumer* m_consumer;

    int64_t m_frame_count { 1 };
    uint8_t* m_io_buffer { nullptr };

    AVCodecContext* m_codec_ctx { nullptr };
    AVFormatContext* m_format_ctx { nullptr };
    AVStream* m_stream { nullptr };
    AVFrame* m_frame { nullptr };
    AVPacket* m_packet {};
};

}
