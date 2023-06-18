#include "lib.h"

#include <unordered_map>
#include <mediasoupclient.hpp>

#include <rtc_base/ssl_adapter.h>
#include <rtc_base/byte_order.h>
#include <api/audio_codecs/builtin_audio_decoder_factory.h>
#include <api/audio_codecs/builtin_audio_encoder_factory.h>
#include <api/create_peerconnection_factory.h>
#include <api/video_codecs/builtin_video_decoder_factory.h>
#include <api/video_codecs/builtin_video_encoder_factory.h>
#include <system_wrappers/include/clock.h>
#include <libyuv.h>

namespace impl {

class Device
    : public mediasoupclient::SendTransport::Listener
    , public mediasoupclient::RecvTransport::Listener
    , public mediasoupclient::Producer::Listener
    , public mediasoupclient::Consumer::Listener {
public:
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
    ::OnConnect on_connect_handler { nullptr };
    ::OnConnectionStateChange on_connection_state_change_handler { nullptr };
    ::OnProduce on_produce_handler { nullptr };
    ::OnSendTransportClose on_send_transport_close_handler { nullptr };
    ::OnRecvTransportClose on_recv_transport_close_handler { nullptr };

    std::unique_ptr<mediasoupclient::SendTransport> send_transport { nullptr };
    std::unique_ptr<mediasoupclient::RecvTransport> recv_transport { nullptr };
    mediasoupclient::Device device {};
private:
    std::unordered_map<int64_t, std::promise<std::string>> m_map_promise_producer_id {};
    std::atomic_int64_t m_map_id_gen {};
};

std::future<void> Device::OnConnect(mediasoupclient::Transport* transport, const nlohmann::json& dtlsParameters)
{
    if (on_connect_handler) {
        auto str = dtlsParameters.dump();
        on_connect_handler(
            reinterpret_cast<::MscDevice*>(this),
            reinterpret_cast<::MscTransport*>(transport),
            str.c_str()
        );
    }

    return {};
}

void Device::OnConnectionStateChange(mediasoupclient::Transport* transport, const std::string& connectionState)
{
    if (on_connection_state_change_handler) {
        on_connection_state_change_handler(
            reinterpret_cast<::MscDevice*>(this),
            reinterpret_cast<::MscTransport*>(transport),
            connectionState.c_str()
        );
    }
}

std::future<std::string> Device::OnProduce(mediasoupclient::SendTransport* transport, const std::string& kind, nlohmann::json rtpParameters, const nlohmann::json&)
{
    std::promise<std::string> promise;
    if (on_produce_handler) {
        auto future = promise.get_future();
        auto promise_id = m_map_id_gen.fetch_add(1);
        m_map_promise_producer_id.insert({ promise_id, std::move(promise) });

        auto str = rtpParameters.dump();
        on_produce_handler(
            reinterpret_cast<::MscDevice*>(this),
            reinterpret_cast<::MscTransport*>(transport),
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
    if (on_send_transport_close_handler) {
        on_send_transport_close_handler(
            reinterpret_cast<::MscDevice*>(this),
            reinterpret_cast<::MscTransport*>(send_transport.get()),
            reinterpret_cast<::MscProducer*>(producer)
        );
    }
}

void Device::OnTransportClose(mediasoupclient::Consumer* consumer)
{
    if (on_recv_transport_close_handler) {
        on_recv_transport_close_handler(
            reinterpret_cast<::MscDevice*>(this),
            reinterpret_cast<::MscTransport*>(recv_transport.get()),
            reinterpret_cast<::MscConsumer*>(consumer)
        );
    }
}

class AudioConsumer : public webrtc::AudioTrackSinkInterface
{
public:
    AudioConsumer(OnAudioData on_audio_data)
        : m_on_audio_data(on_audio_data)
    {
    }

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

        m_on_audio_data(audio_data);
    }
private:
    OnAudioData m_on_audio_data;
};

class VideoConsumer : public rtc::VideoSinkInterface<webrtc::VideoFrame>
{
public:
    VideoConsumer(OnVideoFrame on_video_data)
        : m_on_video_frame(on_video_data)
    {
    }

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

        m_on_video_frame(video_frame);
    }
private:
    OnVideoFrame m_on_video_frame;
    std::vector<uint8_t> m_data {};
};

class AudioProducer : public webrtc::AudioSourceInterface
{
public:
private:
};

class VideoProducer : public webrtc::VideoTrackSourceInterface
{
public:
private:
};

}

static const char* alloc_string(const std::string& str)
{
    char* ret = new char[str.size() + 1];
    std::memcpy(ret, str.c_str(), str.size() + 1);
    return ret;
}

extern "C" {

void msc_free_string(const char* str)
{
    delete[] str;
}

void msc_initialize()
{
    rtc::InitializeSSL();
    rtc::InitRandom(static_cast<int>(rtc::TimeMillis()));
}

void msc_cleanup()
{
    rtc::CleanupSSL();
}

const char* msc_version()
{
    return alloc_string(mediasoupclient::Version());
}

MscDevice* msc_alloc_device()
{
    auto device = new (std::nothrow) impl::Device();
    return reinterpret_cast<MscDevice*>(device);
}

void msc_free_device(MscDevice* device)
{
    delete reinterpret_cast<impl::Device*>(device);
}

const char* msc_get_rtp_capabilities(MscDevice* in_device)
{
    auto* device = reinterpret_cast<impl::Device*>(in_device);
    return alloc_string(device->device.GetRtpCapabilities());
}

bool msc_is_loaded(MscDevice* in_device)
{
    auto* device = reinterpret_cast<impl::Device*>(in_device);
    return device->device.IsLoaded();
}

bool msc_load(MscDevice* in_device, const char* router_rtp_capabilities)
{
    auto* device = reinterpret_cast<impl::Device*>(in_device);
    try {
        device->device.Load(nlohmann::json::parse(router_rtp_capabilities));
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool msc_can_produce(MscDevice* in_device, MscMediaKind kind)
{
    auto* device = reinterpret_cast<impl::Device*>(in_device);
    return device->device.CanProduce(kind == MscMediaKind::Audio ? "audio" : "video");
}

void msc_set_on_connect_handler(MscDevice* in_device, OnConnect handler)
{
    auto* device = reinterpret_cast<impl::Device*>(in_device);
    device->on_connect_handler = handler;
}

void msc_set_on_connection_state_change_handler(MscDevice* in_device, OnConnectionStateChange handler)
{
    auto* device = reinterpret_cast<impl::Device*>(in_device);
    device->on_connection_state_change_handler = handler;
}

void msc_set_on_produce_handler(MscDevice* in_device, OnProduce handler)
{
    auto* device = reinterpret_cast<impl::Device*>(in_device);
    device->on_produce_handler = handler;
}

void msc_set_on_send_transport_close_handler(MscDevice* in_device, OnSendTransportClose handler)
{
    auto* device = reinterpret_cast<impl::Device*>(in_device);
    device->on_send_transport_close_handler = handler;
}

void msc_set_on_recv_transport_close_handler(MscDevice* in_device, OnRecvTransportClose handler)
{
    auto* device = reinterpret_cast<impl::Device*>(in_device);
    device->on_recv_transport_close_handler = handler;
}

void msc_fulfill_producer_id(MscDevice* in_device, int64_t promise_id, const char* producer_id)
{
    auto* device = reinterpret_cast<impl::Device*>(in_device);
    device->fulfill_producer_id(promise_id, producer_id);
}


MscTransport* msc_create_send_transport(
        MscDevice* in_device,
        const char* id,
        const char* ice_parameters,
        const char* ice_candidates,
        const char* dtls_parameters
)
{
    auto* device = reinterpret_cast<impl::Device*>(in_device);

    mediasoupclient::PeerConnection::Options options;
    options.factory = nullptr;

    device->send_transport = std::unique_ptr<mediasoupclient::SendTransport>(
        device->device.CreateSendTransport(
            device,
            id,
            nlohmann::json::parse(ice_parameters),
            nlohmann::json::parse(ice_candidates),
            nlohmann::json::parse(dtls_parameters),
            &options)
    );

    return reinterpret_cast<::MscTransport*>(device->send_transport.get());
}

MscTransport* msc_create_recv_transport(
        MscDevice* in_device,
        const char* id,
        const char* ice_parameters,
        const char* ice_candidates,
        const char* dtls_parameters
)
{
    auto* device = reinterpret_cast<impl::Device*>(in_device);

    mediasoupclient::PeerConnection::Options options;
    options.factory = nullptr;

    device->recv_transport = std::unique_ptr<mediasoupclient::RecvTransport>(
        device->device.CreateRecvTransport(
            device,
            id,
            nlohmann::json::parse(ice_parameters),
            nlohmann::json::parse(ice_candidates),
            nlohmann::json::parse(dtls_parameters),
            &options)
    );

    return reinterpret_cast<::MscTransport*>(device->send_transport.get());
}

MscProducer* msc_create_producer(
        MscDevice* in_device,
        MscTransport* in_transport,
        int encoding_layers,
        const char* codec_options,
        const char* codec
)
{
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

void msc_supply_video(MscDevice*, MscProducer*, MscVideoFrame)
{
    // TODO
}

void msc_supply_audio(MscDevice*, MscProducer*, MscAudioData)
{
    // TODO
}

MscConsumer* msc_create_consumer(
        MscDevice* in_device,
        MscTransport* in_transport,
        const char* id,
        const char* producer_id,
        MscMediaKind kind,
        const char* rtp_parameters
)
{
    auto* device = reinterpret_cast<impl::Device*>(in_device);
    auto* recv_transport = reinterpret_cast<mediasoupclient::RecvTransport*>(in_transport);

    auto parsed_rtp_parameters = nlohmann::json::parse(rtp_parameters);

    auto* consumer = recv_transport->Consume(
        device,
        id,
        producer_id,
        kind == MscMediaKind::Audio ? "audio" : "video",
        &parsed_rtp_parameters);

    return reinterpret_cast<MscConsumer*>(consumer);
}

MscConsumerSink* msc_add_video_sink(MscDevice*, MscConsumer* in_consumer, OnVideoFrame on_video_frame)
{
    auto* consumer = reinterpret_cast<mediasoupclient::Consumer*>(in_consumer);

    auto* sink = new impl::VideoConsumer(on_video_frame);
    dynamic_cast<webrtc::VideoTrackInterface*>(consumer->GetTrack())->AddOrUpdateSink(sink, {});

    return reinterpret_cast<MscConsumerSink*>(sink);
}

MscConsumerSink* msc_add_audio_sink(MscDevice*, MscConsumer* in_consumer, OnAudioData on_audio_data)
{
    auto* consumer = reinterpret_cast<mediasoupclient::Consumer*>(in_consumer);

    auto* sink = new impl::AudioConsumer(on_audio_data);
    dynamic_cast<webrtc::AudioTrackInterface*>(consumer->GetTrack())->AddSink(sink);

    return reinterpret_cast<MscConsumerSink*>(sink);
}

void msc_remove_video_sink(MscDevice*, MscConsumer* in_consumer, MscConsumerSink* in_sink)
{
    auto* consumer = reinterpret_cast<mediasoupclient::Consumer*>(in_consumer);
    auto* sink = reinterpret_cast<impl::VideoConsumer*>(in_sink);

    dynamic_cast<webrtc::VideoTrackInterface*>(consumer->GetTrack())->RemoveSink(sink);
    delete sink;
}

void msc_remove_audio_sink(MscDevice*, MscConsumer* in_consumer, MscConsumerSink* in_sink)
{
    auto* consumer = reinterpret_cast<mediasoupclient::Consumer*>(in_consumer);
    auto* sink = reinterpret_cast<impl::AudioConsumer*>(in_sink);

    dynamic_cast<webrtc::AudioTrackInterface*>(consumer->GetTrack())->RemoveSink(sink);
    delete sink;
}

}
