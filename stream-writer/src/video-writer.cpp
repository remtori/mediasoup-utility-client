#include "./video-writer.hpp"

#include <fmt/format.h>

VideoWriter::VideoWriter(std::string filepath)
    : m_filepath(std::move(filepath))
{
    m_packet = av_packet_alloc();

    m_format_ctx = avformat_alloc_context();
    if (!m_format_ctx) {
        throw std::runtime_error("ffmpeg failed to create format context");
    }

    m_format_ctx->oformat = av_guess_format(nullptr, m_filepath.c_str(), nullptr);

    const AVCodec* codec = nullptr;
    void* iterator = nullptr;
    while ((codec = av_codec_iterate(&iterator)) != nullptr) {
        fmt::println("Codec is_encoder={} name={} long_name={} wrapper_name={}",
            av_codec_is_encoder(codec),
            codec->name == nullptr ? "" : codec->name,
            codec->long_name == nullptr ? "" : codec->long_name,
            codec->wrapper_name == nullptr ? "" : codec->wrapper_name);
    }
}

VideoWriter::~VideoWriter()
{
    if (m_packet) {
        av_packet_free(&m_packet);
    }

    if (m_codec_ctx) {
        avcodec_free_context(&m_codec_ctx);
    }

    if (m_format_ctx) {
        avformat_free_context(m_format_ctx);
    }

    if (m_frame) {
        av_frame_free(&m_frame);
    }
}

void VideoWriter::init_ffmpeg(int width, int height, int fps)
{
    fmt::println("setting up fps={} res={}x{}", fps, width, height);

    m_frame = av_frame_alloc();
    m_frame->format = AV_PIX_FMT_YUV420P;
    m_frame->width = width;
    m_frame->height = height;

    if (av_frame_get_buffer(m_frame, 32) < 0) {
        throw std::runtime_error(std::string("ffmpeg frame_get_buffer failed"));
    }

    // setup_encoder
    const AVCodec* encoder = avcodec_find_encoder(AV_CODEC_ID_VP8);
    if (!encoder) {
        throw std::runtime_error(std::string("ffmpeg failed to find encoder"));
    }

    m_codec_ctx = avcodec_alloc_context3(encoder);
    if (!m_codec_ctx) {
        throw std::runtime_error(std::string("ffmpeg codec_alloc failed"));
    }

    m_codec_ctx->framerate = AVRational { fps, 1 };
    m_codec_ctx->time_base = AVRational { 1, fps };
    m_codec_ctx->gop_size = 12;
    m_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    const_cast<struct AVOutputFormat*>(m_format_ctx->oformat)->video_codec = encoder->id;

    // allocate context for this encoder and set its properties
    m_codec_ctx->bit_rate = 5000000;
    m_codec_ctx->width = width;
    m_codec_ctx->height = height;

    if (avcodec_open2(m_codec_ctx, nullptr, nullptr) < 0) {
        throw std::runtime_error("ffmpeg open codec context failed");
    }

    if (avio_open(&m_format_ctx->pb, m_filepath.c_str(), AVIO_FLAG_WRITE) < 0) {
        throw std::runtime_error("ffmpeg failed to open file");
    }

    m_stream = avformat_new_stream(m_format_ctx, nullptr);
    m_stream->time_base = AVRational { 1, fps };

    if (avcodec_parameters_from_context(m_stream->codecpar, m_codec_ctx) < 0) {
        throw std::runtime_error("ffmpeg copy params failed");
    }

    if (avformat_write_header(m_format_ctx, nullptr) < 0) {
        throw std::runtime_error("ffmpeg write header failed");
    }

    fmt::println("setting up finish ok");
}

void VideoWriter::on_close()
{
    for (;;) {
        avcodec_send_frame(m_codec_ctx, nullptr);
        if (avcodec_receive_packet(m_codec_ctx, m_packet) == 0) {
            av_interleaved_write_frame(m_format_ctx, m_packet);
            av_packet_unref(m_packet);
        } else {
            break;
        }
    }

    av_write_trailer(m_format_ctx);
    int err = avio_close(m_format_ctx->pb);
    if (err < 0) {
        throw std::runtime_error(std::string("Failed to close file") + std::to_string(err));
    }
}

void VideoWriter::on_video_frame(const msc::VideoFrame& frame)
{
    if (m_frame_count == 1)
        init_ffmpeg(frame.width, frame.height, 30);

    m_frame->data[0] = const_cast<uint8_t*>(frame.data_y);
    m_frame->data[1] = const_cast<uint8_t*>(frame.data_u);
    m_frame->data[2] = const_cast<uint8_t*>(frame.data_v);
    m_frame->linesize[0] = frame.stride_y;
    m_frame->linesize[1] = frame.stride_u;
    m_frame->linesize[2] = frame.stride_v;
    m_frame->pts = m_frame_count++;

    if (avcodec_send_frame(m_codec_ctx, m_frame) < 0) {
        throw std::runtime_error("ffmpeg submit frame encoding failed");
    }

    int ret = 0;
    while (ret >= 0) {
        ret = avcodec_receive_packet(m_codec_ctx, m_packet);

        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            throw std::runtime_error(std::string("ffmpeg encoding failed"));
        }

        av_packet_rescale_ts(m_packet, m_codec_ctx->time_base, m_stream->time_base);
        m_packet->stream_index = m_stream->index;

        ret = av_interleaved_write_frame(m_format_ctx, m_packet);
        if (ret < 0) {
            throw std::runtime_error(std::string("ffmpeg writing frame failed"));
        }

        av_packet_unref(m_packet);
    }
}
