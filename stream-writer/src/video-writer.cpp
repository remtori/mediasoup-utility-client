#include "./video-writer.hpp"

#include <fmt/format.h>

VideoWriter::VideoWriter(std::string filepath)
    : m_filepath(std::move(filepath))
{
    // const AVCodec* codec = nullptr;
    // void* iterator = nullptr;
    // while ((codec = av_codec_iterate(&iterator)) != nullptr) {
    //     fmt::println("Codec is_encoder={} name={} long_name={} wrapper_name={}",
    //         av_codec_is_encoder(codec),
    //         codec->name == nullptr ? "" : codec->name,
    //         codec->long_name == nullptr ? "" : codec->long_name,
    //         codec->wrapper_name == nullptr ? "" : codec->wrapper_name);
    // }
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
    char errbuf[64];
    fmt::println("setting up fps={} res={}x{}", fps, width, height);

    AVOutputFormat* output_format = av_guess_format(nullptr, m_filepath.c_str(), nullptr);
    if (!output_format) {
        throw std::runtime_error("ffmpeg failed to guess format");
    }

    avformat_alloc_output_context2(&m_format_ctx, output_format, nullptr, m_filepath.c_str());
    if (!m_format_ctx) {
        throw std::runtime_error("ffmpeg failed to create format context");
    }

    const AVCodec* encoder = avcodec_find_encoder(m_stream->codecpar->codec_id);
    if (!encoder) {
        throw std::runtime_error(std::string("ffmpeg failed to find encoder"));
    }

    m_codec_ctx = avcodec_alloc_context3(encoder);
    if (!m_codec_ctx) {
        throw std::runtime_error(std::string("ffmpeg codec_alloc failed"));
    }

    m_stream = avformat_new_stream(m_format_ctx, nullptr);
    m_stream->codecpar->codec_id = output_format->video_codec;
    m_stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    m_stream->codecpar->width = width;
    m_stream->codecpar->height = height;
    m_stream->codecpar->format = AV_PIX_FMT_YUV420P;
    m_stream->codecpar->bit_rate = 500000;
    avcodec_parameters_to_context(m_codec_ctx, m_stream->codecpar);
    m_codec_ctx->max_b_frames = 2;
    m_codec_ctx->gop_size = 12;
    m_codec_ctx->time_base.num = 1;
    m_codec_ctx->time_base.den = fps;
    m_codec_ctx->framerate.num = fps;
    m_codec_ctx->framerate.den = 1;
    avcodec_parameters_from_context(m_stream->codecpar, m_codec_ctx);

    if (avcodec_open2(m_codec_ctx, encoder, nullptr) < 0) {
        throw std::runtime_error("ffmpeg open codec context failed");
    }

    if (avio_open(&m_format_ctx->pb, m_filepath.c_str(), AVIO_FLAG_WRITE) < 0) {
        throw std::runtime_error("ffmpeg failed to open file");
    }

    if (int err = avformat_write_header(m_format_ctx, nullptr); err < 0) {
        throw std::runtime_error(std::string("ffmpeg write header failed ") + av_make_error_string(errbuf, 64, err));
    }

    m_packet = av_packet_alloc();
    m_frame = av_frame_alloc();
    m_frame->format = AV_PIX_FMT_YUV420P;
    m_frame->width = width;
    m_frame->height = height;

    if (av_frame_get_buffer(m_frame, 32) < 0) {
        throw std::runtime_error(std::string("ffmpeg frame_get_buffer failed"));
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
    m_frame_count++;

    m_frame->data[0] = const_cast<uint8_t*>(frame.data_y);
    m_frame->data[1] = const_cast<uint8_t*>(frame.data_u);
    m_frame->data[2] = const_cast<uint8_t*>(frame.data_v);
    m_frame->linesize[0] = frame.stride_y;
    m_frame->linesize[1] = frame.stride_u;
    m_frame->linesize[2] = frame.stride_v;
    m_frame->pts = frame.timestamp_ms;

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

        m_packet->stream_index = m_stream->index;

        ret = av_interleaved_write_frame(m_format_ctx, m_packet);
        if (ret < 0) {
            throw std::runtime_error(std::string("ffmpeg writing frame failed"));
        }

        av_packet_unref(m_packet);
    }
}
