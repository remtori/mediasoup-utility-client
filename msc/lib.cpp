#include "lib.h"

#include <string_view>
#include <unordered_map>

#include <fmt/core.h>
#include <mediasoupclient.hpp>

#include <libyuv.h>
#include <rtc_base/byte_order.h>
#include <rtc_base/ssl_adapter.h>
#include <system_wrappers/include/clock.h>

static WriteLog s_write_fn = nullptr;

namespace {

template<typename... T>
inline void println(::fmt::format_string<T...> fmt, T&&... args)
{
    if (!s_write_fn)
        return;

    std::string string = ::fmt::vformat(fmt, ::fmt::make_format_args(args...));
    s_write_fn(string.c_str(), string.size());
}

}

namespace impl {

class Device
    : public mediasoupclient::SendTransport::Listener
    , public mediasoupclient::RecvTransport::Listener
    , public mediasoupclient::Producer::Listener
    , public mediasoupclient::Consumer::Listener {
public:
    Device(void* ctx,
        ::OnConnect on_connect_handler,
        ::OnConnectionStateChange on_connection_state_change_handler,
        ::OnProduce on_produce_handler)
        : m_user_ctx(ctx)
        , m_on_connect_handler(on_connect_handler)
        , m_on_connection_state_change_handler(on_connection_state_change_handler)
        , m_on_produce_handler(on_produce_handler)
    {
    }

    std::future<void> OnConnect(mediasoupclient::Transport*, const nlohmann::json&) override;
    void OnConnectionStateChange(mediasoupclient::Transport*, const std::string&) override;
    std::future<std::string> OnProduce(mediasoupclient::SendTransport*, const std::string&, nlohmann::json, const nlohmann::json&) override;
    std::future<std::string> OnProduceData(mediasoupclient::SendTransport*, const nlohmann::json&, const std::string&, const std::string&, const nlohmann::json&) override;
    void OnTransportClose(mediasoupclient::Producer* producer) override;
    void OnTransportClose(mediasoupclient::Consumer* consumer) override;

    void fulfill_producer_id(int64_t promise_id, const std::string& producer_id)
    {
        auto it = m_map_promise_producer_id.find(promise_id);
        it->second.set_value(producer_id);
        m_map_promise_producer_id.erase(it);
    }

public:
    std::unique_ptr<mediasoupclient::SendTransport> send_transport { nullptr };
    std::unique_ptr<mediasoupclient::RecvTransport> recv_transport { nullptr };
    mediasoupclient::Device device {};

private:
    void* m_user_ctx { nullptr };
    ::OnConnect m_on_connect_handler { nullptr };
    ::OnConnectionStateChange m_on_connection_state_change_handler { nullptr };
    ::OnProduce m_on_produce_handler { nullptr };

    std::unordered_map<int64_t, std::promise<std::string>> m_map_promise_producer_id {};
    std::atomic_int64_t m_map_id_gen {};
};

std::future<void> Device::OnConnect(mediasoupclient::Transport* transport, const nlohmann::json& dtlsParameters)
{
    if (m_on_connect_handler) {
        auto str = dtlsParameters.dump();
        m_on_connect_handler(
            reinterpret_cast<::MscDevice*>(this),
            reinterpret_cast<::MscTransport*>(transport),
            m_user_ctx,
            str.c_str());
    }

    std::promise<void> ret;
    ret.set_value();
    return ret.get_future();
}

void Device::OnConnectionStateChange(mediasoupclient::Transport* transport, const std::string& connectionState)
{
    if (m_on_connection_state_change_handler) {
        m_on_connection_state_change_handler(
            reinterpret_cast<::MscDevice*>(this),
            reinterpret_cast<::MscTransport*>(transport),
            m_user_ctx,
            connectionState.c_str());
    }
}

std::future<std::string> Device::OnProduce(mediasoupclient::SendTransport* transport, const std::string& kind, nlohmann::json rtpParameters, const nlohmann::json&)
{
    std::promise<std::string> promise;
    if (m_on_produce_handler) {
        auto future = promise.get_future();
        auto promise_id = m_map_id_gen.fetch_add(1);
        m_map_promise_producer_id.insert({ promise_id, std::move(promise) });

        auto str = rtpParameters.dump();
        m_on_produce_handler(
            reinterpret_cast<::MscDevice*>(this),
            reinterpret_cast<::MscTransport*>(transport),
            m_user_ctx,
            promise_id,
            kind == "audio" ? ::MscMediaKind::Audio : ::MscMediaKind::Video,
            str.c_str());

        return future;
    }

    promise.set_exception(std::make_exception_ptr("OnProduce(): null on_produce_handler handler"));
    return promise.get_future();
}

std::future<std::string> Device::OnProduceData(mediasoupclient::SendTransport*, const nlohmann::json&, const std::string&, const std::string&, const nlohmann::json&)
{
    std::promise<std::string> promise;
    promise.set_exception(std::make_exception_ptr("OnProduceData(): data channel is not supported"));
    return promise.get_future();
}

void Device::OnTransportClose(mediasoupclient::Producer* producer)
{
    // TODO
}

void Device::OnTransportClose(mediasoupclient::Consumer* consumer)
{
    // TODO
}

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
    mediasoupclient::Consumer* consumer() const { return m_consumer; }

    void OnData(
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

    void OnFrame(const webrtc::VideoFrame& frame) override
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

private:
    mediasoupclient::Consumer* m_consumer;
    void* m_user_ctx;
    OnVideoFrame m_on_video_frame;
    std::vector<uint8_t> m_data {};
};

class AudioProducer : public webrtc::AudioSourceInterface {
public:
private:
};

class VideoProducer : public webrtc::VideoTrackSourceInterface {
public:
private:
};

class FFIMediasoupLogHandler : public mediasoupclient::Logger::LogHandlerInterface {
public:
    void OnLog(mediasoupclient::Logger::LogLevel level, char* payload, size_t len) override
    {
        println("[MS]({}): {}", static_cast<int>(level), std::string_view(payload, len));
    }
};

class FFIWebrtcLogSink : public rtc::LogSink {
    void OnLogMessage(const std::string& message) override
    {
        println("[RTC] {}", message);
    }
};

webrtc::PeerConnectionFactoryInterface* peer_connection_factory();

}

static const char* alloc_string(const std::string& str) noexcept
{
    char* ret = new char[str.size() + 1];
    std::memcpy(ret, str.c_str(), str.size() + 1);
    return ret;
}

extern "C" {

void msc_free_string(const char* str) noexcept
{
    delete[] str;
}

void msc_initialize(WriteLog write_fn) noexcept
{
    s_write_fn = write_fn;

    println("initialize() start");

    rtc::InitializeSSL();
    rtc::InitRandom(static_cast<int>(rtc::TimeMillis()));
    // rtc::LogMessage::LogToDebug(rtc::LS_INFO);
    // rtc::LogMessage::AddLogToStream(new impl::FFIWebrtcLogSink(), rtc::LS_INFO);

    mediasoupclient::Logger::SetLogLevel(mediasoupclient::Logger::LogLevel::LOG_WARN);
    mediasoupclient::Logger::SetHandler(new impl::FFIMediasoupLogHandler());

    impl::peer_connection_factory();

    println("initialize() end");
}

void msc_cleanup() noexcept
{
    println("cleanup()");

    rtc::CleanupSSL();
}

const char* msc_version() noexcept
{
    println("version()");
    return alloc_string(mediasoupclient::Version());
}

MscDevice* msc_alloc_device(void* ctx,
    ::OnConnect on_connect_handler,
    ::OnConnectionStateChange on_connection_state_change_handler,
    ::OnProduce on_produce_handler) noexcept
{
    println("new Device()");
    auto device = new (std::nothrow) impl::Device(ctx,
        on_connect_handler,
        on_connection_state_change_handler,
        on_produce_handler);

    return reinterpret_cast<MscDevice*>(device);
}

void msc_free_device(MscDevice* device) noexcept
{
    println("delete Device");
    delete reinterpret_cast<impl::Device*>(device);
}

const char* msc_get_rtp_capabilities(MscDevice* in_device) noexcept
{
    println("Device::get_rtp_capabilities");
    auto* device = reinterpret_cast<impl::Device*>(in_device);
    return alloc_string(device->device.GetRtpCapabilities().dump());
}

bool msc_load(MscDevice* in_device, const char* router_rtp_capabilities) noexcept
{
    println("Device::load()");

    mediasoupclient::PeerConnection::Options options;
    options.factory = impl::peer_connection_factory();

    auto* device = reinterpret_cast<impl::Device*>(in_device);
    device->device.Load(nlohmann::json::parse(router_rtp_capabilities), &options);
    return true;
}

bool msc_can_produce(MscDevice* in_device, MscMediaKind kind) noexcept
{
    println("Device::can_produce()");
    auto* device = reinterpret_cast<impl::Device*>(in_device);
    return device->device.CanProduce(kind == MscMediaKind::Audio ? "audio" : "video");
}

void msc_fulfill_producer_id(MscDevice* in_device, int64_t promise_id, const char* producer_id) noexcept
{
    println("Device::fulfill_producer_id()");
    auto* device = reinterpret_cast<impl::Device*>(in_device);
    device->fulfill_producer_id(promise_id, producer_id);
}

const char* msc_transport_get_id(MscTransport* in_transport) noexcept
{
    println("Transport::get_id()");
    auto* transport = reinterpret_cast<mediasoupclient::Transport*>(in_transport);
    return alloc_string(transport->GetId());
}

const void* msc_transport_get_ctx(MscTransport* in_transport) noexcept
{
    println("Transport::get_ctx()");
    auto* transport = reinterpret_cast<mediasoupclient::Transport*>(in_transport);
    return reinterpret_cast<void*>(transport->GetAppData().value("ctx", (uintptr_t)0));
}

MscTransport* msc_create_transport(
    MscDevice* in_device,
    bool is_send,
    const char* id,
    const char* ice_parameters,
    const char* ice_candidates,
    const char* dtls_parameters,
    void* transport_ctx) noexcept
{
    println("Device::create_transport(is_send={})", is_send);
    auto* device = reinterpret_cast<impl::Device*>(in_device);

    mediasoupclient::PeerConnection::Options options;
    options.factory = impl::peer_connection_factory();

    nlohmann::json user_data = {
        { "ctx", reinterpret_cast<uintptr_t>(transport_ctx) }
    };

    mediasoupclient::Transport* transport;
    if (is_send) {
        transport = device->device.CreateSendTransport(
            device,
            id,
            nlohmann::json::parse(ice_parameters),
            nlohmann::json::parse(ice_candidates),
            nlohmann::json::parse(dtls_parameters),
            &options,
            user_data);
    } else {
        transport = device->device.CreateRecvTransport(
            device,
            id,
            nlohmann::json::parse(ice_parameters),
            nlohmann::json::parse(ice_candidates),
            nlohmann::json::parse(dtls_parameters),
            &options,
            user_data);
    }

    return reinterpret_cast<MscTransport*>(transport);
}

void msc_free_transport(MscTransport* in_transport) noexcept
{
    println("delete Transport");
    auto* transport = reinterpret_cast<mediasoupclient::Transport*>(in_transport);
    transport->Close();
    delete transport;
}

MscProducer* msc_create_producer(
    MscDevice* in_device,
    MscTransport* in_transport,
    int encoding_layers,
    const char* codec_options,
    const char* codec) noexcept
{
    println("Transport::create_producer()");

    auto* device = reinterpret_cast<impl::Device*>(in_device);
    auto* send_transport = reinterpret_cast<mediasoupclient::SendTransport*>(in_transport);

    std::vector<webrtc::RtpEncodingParameters> encodings;
    for (int i = 0; i < encoding_layers; i++) {
        encodings.emplace_back();
    }

    auto parsed_codec_options = nlohmann::json::parse(codec_options);
    auto parsed_codec = nlohmann::json::parse(codec);

    // TODO
    return nullptr;
    //    auto* producer = send_transport->Produce(
    //        device,
    //        nullptr,
    //        &encodings,
    //        &parsed_codec_options,
    //        &parsed_codec,
    //        nlohmann::json::object()
    //    );
    //
    //    return new (std::nothrow) ::Producer {
    //        .producer = producer,
    //        .source = source,
    //    };
}

void msc_free_producer(MscProducer* in_producer) noexcept
{
    println("delete Producer");

    auto* producer = reinterpret_cast<mediasoupclient::Producer*>(in_producer);
    producer->Close();

    delete producer;
}

void msc_supply_video(MscDevice*, MscProducer*, MscVideoFrame) noexcept
{
    println("Producer::supply_video()");
    // TODO
}

void msc_supply_audio(MscDevice*, MscProducer*, MscAudioData) noexcept
{
    println("Producer::supply_audio()");
    // TODO
}

MscConsumerSink* msc_create_video_sink(
    MscDevice* in_device,
    MscTransport* in_transport,
    const char* id,
    const char* producer_id,
    const char* rtp_parameters,
    void* user_ctx,
    OnVideoFrame on_video_frame) noexcept
{
    println("Transport::create_video_sink()");

    auto* device = reinterpret_cast<impl::Device*>(in_device);
    auto* recv_transport = reinterpret_cast<mediasoupclient::RecvTransport*>(in_transport);

    auto parsed_rtp_parameters = nlohmann::json::parse(rtp_parameters);
    auto* consumer = recv_transport->Consume(
        device,
        id,
        producer_id,
        "video",
        &parsed_rtp_parameters);

    auto* sink = new impl::VideoSink(consumer, user_ctx, on_video_frame);
    dynamic_cast<webrtc::VideoTrackInterface*>(consumer->GetTrack())->AddOrUpdateSink(sink, {});

    return reinterpret_cast<MscConsumerSink*>(sink);
}

MscConsumerSink* msc_create_audio_sink(
    MscDevice* in_device,
    MscTransport* in_transport,
    const char* id,
    const char* producer_id,
    const char* rtp_parameters,
    void* user_ctx,
    OnAudioData on_audio_data) noexcept
{
    println("Transport::create_audio_sink()");

    auto* device = reinterpret_cast<impl::Device*>(in_device);
    auto* recv_transport = reinterpret_cast<mediasoupclient::RecvTransport*>(in_transport);

    auto parsed_rtp_parameters = nlohmann::json::parse(rtp_parameters);
    auto* consumer = recv_transport->Consume(
        device,
        id,
        producer_id,
        "audio",
        &parsed_rtp_parameters);

    auto* sink = new impl::AudioSink(consumer, user_ctx, on_audio_data);
    dynamic_cast<webrtc::AudioTrackInterface*>(consumer->GetTrack())->AddSink(sink);

    return reinterpret_cast<MscConsumerSink*>(sink);
}

void msc_free_sink(MscConsumerSink* in_sink) noexcept
{
    println("delete ConsumerSink");

    auto* sink = reinterpret_cast<impl::BaseSink*>(in_sink);
    auto* consumer = sink->consumer();
    if (sink->is_audio_sink()) {
        auto* audio_sink = dynamic_cast<impl::AudioSink*>(sink);
        dynamic_cast<webrtc::AudioTrackInterface*>(consumer->GetTrack())->RemoveSink(audio_sink);
        delete audio_sink;
    } else {
        auto* video_sink = dynamic_cast<impl::VideoSink*>(sink);
        dynamic_cast<webrtc::VideoTrackInterface*>(consumer->GetTrack())->RemoveSink(video_sink);
        delete video_sink;
    }

    consumer->Close();
    delete consumer;
}
}
