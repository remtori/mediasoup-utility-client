#include "lib.h"

#include "./common.hpp"
#include "./device.hpp"
#include "./media_sink.hpp"
#include "./video_writer.hpp"

#include <string_view>
#include <unordered_map>

#include <json.hpp>

#include <MediaSoupClientErrors.hpp>
#include <mediasoupclient.hpp>

#include <rtc_base/byte_order.h>
#include <rtc_base/ssl_adapter.h>
#include <system_wrappers/include/clock.h>

WriteLog s_write_fn = nullptr;
static PushError s_push_error_fn = nullptr;

namespace {

static void handleError()
{
    if (!s_push_error_fn) {
        println("ERROR. EXCEPTION CAUGHT. But no push_error function found so the error will be silently ignore.");
    }

    try {
        throw;
    } catch (const ::MediaSoupClientTypeError& ex) {
        s_push_error_fn(::MscErrorKind::MediasoupTypeError, ex.what());
    } catch (const ::MediaSoupClientUnsupportedError& ex) {
        s_push_error_fn(::MscErrorKind::MediasoupUnsupportedError, ex.what());
    } catch (const ::MediaSoupClientInvalidStateError& ex) {
        s_push_error_fn(::MscErrorKind::MediasoupInvalidStateError, ex.what());
    } catch (const ::MediaSoupClientError& ex) {
        s_push_error_fn(::MscErrorKind::MediasoupOther, ex.what());
    } catch (const nlohmann::json::parse_error& ex) {
        s_push_error_fn(::MscErrorKind::JsonParseError, ex.what());
    } catch (const nlohmann::json::invalid_iterator& ex) {
        s_push_error_fn(::MscErrorKind::JsonInvalidIter, ex.what());
    } catch (const nlohmann::json::type_error& ex) {
        s_push_error_fn(::MscErrorKind::JsonTypeError, ex.what());
    } catch (const nlohmann::json::out_of_range& ex) {
        s_push_error_fn(::MscErrorKind::JsonOutOfRange, ex.what());
    } catch (const nlohmann::json::other_error& ex) {
        s_push_error_fn(::MscErrorKind::JsonOther, ex.what());
    } catch (const std::exception& ex) {
        s_push_error_fn(::MscErrorKind::Exception, ex.what());
    } catch (...) {
        s_push_error_fn(::MscErrorKind::Unknown, nullptr);
    }
}

}

namespace impl {

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

void msc_initialize(WriteLog write_fn, PushError push_error_fn) noexcept
{
    s_write_fn = write_fn;
    s_push_error_fn = push_error_fn;

    try {
        println("initialize() start");

        rtc::InitializeSSL();
        rtc::InitRandom(static_cast<int>(rtc::TimeMillis()));
        // rtc::LogMessage::LogToDebug(rtc::LS_INFO);
        // rtc::LogMessage::AddLogToStream(new impl::FFIWebrtcLogSink(), rtc::LS_INFO);

        mediasoupclient::Logger::SetLogLevel(mediasoupclient::Logger::LogLevel::LOG_WARN);
        mediasoupclient::Logger::SetHandler(new impl::FFIMediasoupLogHandler());

        impl::peer_connection_factory();

        println("initialize() end");
    } catch (...) {
        handleError();
    }
}

void msc_cleanup() noexcept
{
    try {
        println("cleanup()");

        rtc::CleanupSSL();
    } catch (...) {
        handleError();
    }
}

const char* msc_version() noexcept
{
    try {
        println("version()");
        return alloc_string(mediasoupclient::Version());
    } catch (...) {
        handleError();
        return nullptr;
    }
}

MscDevice* msc_alloc_device(void* ctx,
    ::OnConnect on_connect_handler,
    ::OnConnectionStateChange on_connection_state_change_handler,
    ::OnProduce on_produce_handler) noexcept
{
    try {
        println("new Device()");
        auto device = new (std::nothrow) impl::Device(ctx,
            on_connect_handler,
            on_connection_state_change_handler,
            on_produce_handler);

        return reinterpret_cast<MscDevice*>(device);
    } catch (...) {
        handleError();
        return nullptr;
    }
}

void msc_free_device(MscDevice* device) noexcept
{
    try {
        println("delete Device");
        delete reinterpret_cast<impl::Device*>(device);
    } catch (...) {
        handleError();
    }
}

const char* msc_get_rtp_capabilities(MscDevice* in_device) noexcept
{
    try {
        println("Device::get_rtp_capabilities");
        auto* device = reinterpret_cast<impl::Device*>(in_device);
        return alloc_string(device->device().GetRtpCapabilities().dump());
    } catch (...) {
        handleError();
        return nullptr;
    }
}

bool msc_load(MscDevice* in_device, const char* router_rtp_capabilities) noexcept
{
    try {
        println("Device::load()");

        mediasoupclient::PeerConnection::Options options;
        options.factory = impl::peer_connection_factory();

        auto* device = reinterpret_cast<impl::Device*>(in_device);
        device->device().Load(nlohmann::json::parse(router_rtp_capabilities), &options);
        return true;
    } catch (...) {
        handleError();
        return false;
    }
}

bool msc_can_produce(MscDevice* in_device, MscMediaKind kind) noexcept
{
    try {
        println("Device::can_produce()");
        auto* device = reinterpret_cast<impl::Device*>(in_device);
        return device->device().CanProduce(kind == MscMediaKind::Audio ? "audio" : "video");
    } catch (...) {
        handleError();
        return false;
    }
}

void msc_fulfill_producer_id(MscDevice* in_device, int64_t promise_id, const char* producer_id) noexcept
{
    try {
        println("Device::fulfill_producer_id()");
        auto* device = reinterpret_cast<impl::Device*>(in_device);
        device->fulfill_producer_id(promise_id, producer_id);
    } catch (...) {
        handleError();
    }
}

const char* msc_transport_get_id(MscTransport* in_transport) noexcept
{
    try {
        println("Transport::get_id()");
        auto* transport = reinterpret_cast<mediasoupclient::Transport*>(in_transport);
        return alloc_string(transport->GetId());
    } catch (...) {
        handleError();
        return nullptr;
    }
}

const void* msc_transport_get_ctx(MscTransport* in_transport) noexcept
{
    try {
        println("Transport::get_ctx()");
        auto* transport = reinterpret_cast<mediasoupclient::Transport*>(in_transport);
        return reinterpret_cast<void*>(transport->GetAppData().value("ctx", (uintptr_t)0));
    } catch (...) {
        handleError();
        return nullptr;
    }
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
    try {
        println("Device::create_transport(is_send={})", is_send);
        auto* device = reinterpret_cast<impl::Device*>(in_device);

        mediasoupclient::PeerConnection::Options options;
        options.factory = impl::peer_connection_factory();

        nlohmann::json user_data = {
            { "ctx", reinterpret_cast<uintptr_t>(transport_ctx) }
        };

        mediasoupclient::Transport* transport;
        if (is_send) {
            transport = device->device().CreateSendTransport(
                device,
                id,
                nlohmann::json::parse(ice_parameters),
                nlohmann::json::parse(ice_candidates),
                nlohmann::json::parse(dtls_parameters),
                &options,
                user_data);
        } else {
            transport = device->device().CreateRecvTransport(
                device,
                id,
                nlohmann::json::parse(ice_parameters),
                nlohmann::json::parse(ice_candidates),
                nlohmann::json::parse(dtls_parameters),
                &options,
                user_data);
        }

        return reinterpret_cast<MscTransport*>(transport);
    } catch (...) {
        handleError();
        return nullptr;
    }
}

void msc_free_transport(MscTransport* in_transport) noexcept
{
    try {
        println("delete Transport");
        auto* transport = reinterpret_cast<mediasoupclient::Transport*>(in_transport);
        transport->Close();
        delete transport;
    } catch (...) {
        handleError();
    }
}

MscProducer* msc_create_producer(
    MscDevice* in_device,
    MscTransport* in_transport,
    int encoding_layers,
    const char* codec_options,
    const char* codec) noexcept
{
    try {
        println("Transport::create_producer()");

        (void)in_device;
        (void)in_transport;
        (void)encoding_layers;
        (void)codec_options;
        (void)codec;
        // auto* device = reinterpret_cast<impl::Device*>(in_device);
        // auto* send_transport = reinterpret_cast<mediasoupclient::SendTransport*>(in_transport);

        // std::vector<webrtc::RtpEncodingParameters> encodings;
        // for (int i = 0; i < encoding_layers; i++) {
        //     encodings.emplace_back();
        // }

        // auto parsed_codec_options = nlohmann::json::parse(codec_options);
        // auto parsed_codec = nlohmann::json::parse(codec);

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
    } catch (...) {
        handleError();
        return nullptr;
    }
}

void msc_free_producer(MscProducer* in_producer) noexcept
{
    try {
        println("delete Producer");

        auto* producer = reinterpret_cast<mediasoupclient::Producer*>(in_producer);
        producer->Close();

        delete producer;
    } catch (...) {
        handleError();
    }
}

void msc_supply_video(MscDevice*, MscProducer*, MscVideoFrame) noexcept
{
    try {
        println("Producer::supply_video()");
        // TODO
    } catch (...) {
        handleError();
    }
}

void msc_supply_audio(MscDevice*, MscProducer*, MscAudioData) noexcept
{
    try {
        println("Producer::supply_audio()");
        // TODO
    } catch (...) {
        handleError();
    }
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
    try {
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
    } catch (...) {
        handleError();
        return nullptr;
    }
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
    try {
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
    } catch (...) {
        handleError();
        return nullptr;
    }
}

MscConsumerSink* msc_create_video_writer(
    MscDevice* in_device,
    MscTransport* in_transport,
    const char* id,
    const char* producer_id,
    const char* rtp_parameters,
    void* ctx,
    OnVideoWrite callback) noexcept
{
    try {
        println("Transport::create_video_writer()");

        auto* device = reinterpret_cast<impl::Device*>(in_device);
        auto* recv_transport = reinterpret_cast<mediasoupclient::RecvTransport*>(in_transport);

        auto parsed_rtp_parameters = nlohmann::json::parse(rtp_parameters);
        auto* consumer = recv_transport->Consume(
            device,
            id,
            producer_id,
            "video",
            &parsed_rtp_parameters);

        auto* sink = new impl::VideoWriter(consumer, ctx, callback);
        dynamic_cast<webrtc::VideoTrackInterface*>(consumer->GetTrack())->AddOrUpdateSink(sink, {});

        return reinterpret_cast<MscConsumerSink*>(sink);
    } catch (...) {
        handleError();
        return nullptr;
    }
}

void msc_free_sink(MscConsumerSink* in_sink) noexcept
{
    try {
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
    } catch (...) {
        handleError();
    }
}
}
