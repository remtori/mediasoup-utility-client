#include "./video_writer.hpp"

#include "./common.hpp"

#include <stdexcept>

static const int io_buffer_size = 4096 * 4;

namespace impl {

VideoWriter::VideoWriter(mediasoupclient::Consumer* consumer, void* user_ctx, WriteFn write_fn)
    : m_consumer(consumer)
{
    m_packet = av_packet_alloc();
    m_io_buffer = static_cast<uint8_t*>(av_malloc(io_buffer_size));

    m_format_ctx = avformat_alloc_context();
    if (!m_format_ctx) {
        throw std::runtime_error("ffmpeg failed to create format context");
    }

    m_format_ctx->pb = avio_alloc_context(m_io_buffer, io_buffer_size, 0, user_ctx, nullptr, write_fn, nullptr);
    if (!m_format_ctx->pb) {
        throw std::runtime_error("ffmpeg avio_alloc_context() failed");
    }

    m_format_ctx->oformat = av_guess_format(nullptr, "output.mp4", nullptr);

    const AVCodec* codec = nullptr;
    void* iterator = nullptr;
    while ((codec = av_codec_iterate(&iterator)) != nullptr) {
        println("Codec is_encoder={} name={} long_name={} wrapper_name={}",
            av_codec_is_encoder(codec),
            codec->name == nullptr ? "" : codec->name,
            codec->long_name == nullptr ? "" : codec->long_name,
            codec->wrapper_name == nullptr ? "" : codec->wrapper_name);
    }
}

VideoWriter::~VideoWriter()
{
    avcodec_free_context(&m_codec_ctx);
    avformat_free_context(m_format_ctx);
    av_frame_free(&m_frame);
    av_packet_free(&m_packet);
    av_free(m_io_buffer);
}

void VideoWriter::setup(int fps, int width, int height)
{
    println("setting up fps={} res={}x{}", fps, width, height);

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
        throw std::runtime_error(std::string("ffmpeg open codec context failed"));
    }

    m_stream = avformat_new_stream(m_format_ctx, nullptr);
    m_stream->time_base = AVRational { 1, fps };

    if (avcodec_parameters_from_context(m_stream->codecpar, m_codec_ctx) < 0) {
        throw std::runtime_error(std::string("ffmpeg copy params failed"));
    }

    if (int ret = avformat_write_header(m_format_ctx, nullptr); ret < 0) {
        throw std::runtime_error(std::string("ffmpeg write header failed"));
    }

    println("setting up finish ok");
}

void VideoWriter::OnFrame(const webrtc::VideoFrame& frame)
{
    if (m_frame_count == 1)
        setup(30, frame.width(), frame.height());

    println("got a frame");
    auto i420_buffer = frame.video_frame_buffer()->ToI420();

    m_frame->data[0] = const_cast<uint8_t*>(i420_buffer->DataY());
    m_frame->data[1] = const_cast<uint8_t*>(i420_buffer->DataU());
    m_frame->data[2] = const_cast<uint8_t*>(i420_buffer->DataV());
    m_frame->linesize[0] = i420_buffer->StrideY();
    m_frame->linesize[1] = i420_buffer->StrideU();
    m_frame->linesize[2] = i420_buffer->StrideV();
    m_frame->pts = m_frame_count;

    int ret = avcodec_send_frame(m_codec_ctx, m_frame);
    if (ret < 0) {
        throw std::runtime_error(std::string("ffmpeg submit frame encoding failed"));
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(m_codec_ctx, m_packet);

        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            throw std::runtime_error(std::string("ffmpeg encoding failed"));
        }

        av_packet_rescale_ts(m_packet, m_codec_ctx->time_base, m_stream->time_base);
        m_packet->stream_index = m_stream->index;
        m_packet->pts = m_frame_count++;
        ret = av_interleaved_write_frame(m_format_ctx, m_packet);
        if (ret < 0) {
            throw std::runtime_error(std::string("ffmpeg writing frame failed"));
        }

        av_packet_unref(m_packet);
    }
}

}