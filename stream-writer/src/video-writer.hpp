#include <memory>
#include <string>

#include <msc/msc.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
}

class VideoWriter : public msc::VideoConsumer {
public:
    VideoWriter(std::string filepath);
    ~VideoWriter();

    void on_video_frame(const msc::VideoFrame&) override;
    void on_close() override;

private:
    void init_ffmpeg(int width, int height, int fps);

private:
    std::string m_filepath;
    int64_t m_frame_count { 1 };

    AVCodecContext* m_codec_ctx { nullptr };
    AVFormatContext* m_format_ctx { nullptr };

    AVStream* m_stream { nullptr };
    AVFrame* m_frame { nullptr };
    AVPacket* m_packet { nullptr };
};
