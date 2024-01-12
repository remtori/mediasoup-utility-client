#include "msc/msc.hpp"

#include "./media_sender.hpp"
#include "./media_sink.hpp"
#include "./peer_connection_factory.hpp"
#include "./serde.hpp"

#include <variant>

#include <MediaSoupClientErrors.hpp>
#include <mediasoupclient.hpp>

#include <rtc_base/byte_order.h>
#include <rtc_base/ssl_adapter.h>
#include <system_wrappers/include/clock.h>

namespace msc {

static const ::nlohmann::json s_null_json = nullptr;
webrtc::PeerConnectionFactoryInterface* peer_connection_factory();

namespace {

const std::string kAudio = "audio";
const std::string kVideo = "video";

class FFIMediasoupLogHandler : public mediasoupclient::Logger::LogHandlerInterface {
public:
    void OnLog(mediasoupclient::Logger::LogLevel, char* payload, size_t size) override
    {
        cm::log("[MS]{}", std::string_view(payload, size));
    }
};

class DeviceImpl : public Device
    , public mediasoupclient::SendTransport::Listener
    , public mediasoupclient::RecvTransport::Listener
    , public mediasoupclient::Producer::Listener
    , public mediasoupclient::Consumer::Listener
    , public mediasoupclient::DataProducer::Listener {
public:
    DeviceImpl(DeviceDelegate* delegate, rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory)
        : m_delegate(delegate)
        , m_peer_connection_factory(std::move(peer_connection_factory))
    {
    }

    ~DeviceImpl() override
    {
        DeviceImpl::stop();
    }

    bool load(const nlohmann::json& rtp_capabilities) noexcept override
    {
        if (m_device.IsLoaded())
            return false;

        mediasoupclient::PeerConnection::Options options;
        options.factory = m_peer_connection_factory.get();

        m_device.Load(rtp_capabilities, &options);

        return true;
    }

    const nlohmann::json& rtp_capabilities() const noexcept override
    {
        if (!m_device.IsLoaded())
            return s_null_json;

        return m_device.GetRtpCapabilities();
    }

    void stop() noexcept override
    {
        m_sinks.clear();
        if (m_send_transport)
            m_send_transport->Close();
        m_send_transport = nullptr;

        if (m_recv_transport)
            m_recv_transport->Close();
        m_recv_transport = nullptr;
    }

    bool can_produce(MediaKind kind) noexcept override
    {
        if (!m_device.IsLoaded())
            return false;

        return m_device.CanProduce(kind == MediaKind::Audio ? kAudio : kAudio);
    }

    void ensure_transport(TransportKind kind) noexcept override;

    void create_video_sink(const ConsumerOptions&, std::shared_ptr<VideoConsumer> consumer) noexcept override;
    void create_audio_sink(const ConsumerOptions&, std::shared_ptr<AudioConsumer> consumer) noexcept override;
    void create_data_sink(const std::string& consumer_id, const std::string& producer_id, uint16_t stream_id, const std::string& label, const std::string& protocol, std::shared_ptr<DataConsumer>) noexcept override;

    void close_video_sink(const std::shared_ptr<VideoConsumer>& consumer) noexcept override { close_sink(consumer.get()); }
    void close_audio_sink(const std::shared_ptr<AudioConsumer>& consumer) noexcept override { close_sink(consumer.get()); }
    void close_data_sink(const std::shared_ptr<DataConsumer>&) noexcept override;

    std::shared_ptr<VideoSender> create_video_source(const ProducerOptions&) noexcept override;
    std::shared_ptr<AudioSender> create_audio_source(const ProducerOptions&) noexcept override;
    std::shared_ptr<DataSender> create_data_source(const std::string& label, const std::string& protocol, bool ordered, int maxRetransmits, int maxPacketLifeTime) noexcept override;

    std::shared_ptr<void> re_encode(MediaKind, const ConsumerOptions&, const ProducerOptions&) noexcept override;

private:
    void close_sink(const void* consumer) noexcept;
    void close_sender(const void* producer) noexcept;

public:
    std::future<void> OnConnect(mediasoupclient::Transport* transport, const nlohmann::json& dtls_parameters) override
    {
        return m_delegate->connect_transport(
            transport == m_send_transport.get() ? TransportKind::Send : TransportKind::Recv,
            transport->GetId(),
            dtls_parameters);
    }

    std::future<std::string> OnProduce(mediasoupclient::SendTransport* transport, const std::string& kind, nlohmann::json rtp_parameters, const nlohmann::json& app_data) override
    {
        (void)app_data;
        return m_delegate->connect_producer(transport->GetId(), kind == kAudio ? MediaKind::Audio : MediaKind::Video, rtp_parameters);
    }

    std::future<std::string> OnProduceData(mediasoupclient::SendTransport* transport, const nlohmann::json& sctp_parameters, const std::string& label, const std::string& protocol, const nlohmann::json& app_data) override
    {
        (void)app_data;
        return m_delegate->connect_data_producer(transport->GetId(), sctp_parameters, label, protocol);
    }

    void OnTransportClose(mediasoupclient::Producer* producer) override { close_sender(producer); }
    void OnTransportClose(mediasoupclient::DataProducer* producer) override { close_sender(producer); }
    void OnTransportClose(mediasoupclient::Consumer* consumer) override;

    void OnConnectionStateChange(mediasoupclient::Transport* transport, const std::string& state) override
    {
        m_delegate->on_connection_state_change(
            transport == m_send_transport.get() ? TransportKind::Send : TransportKind::Recv,
            transport->GetId(),
            state);
    }

    void OnOpen(mediasoupclient::DataProducer*) override { }
    void OnClose(mediasoupclient::DataProducer*) override { }
    void OnBufferedAmountChange(mediasoupclient::DataProducer*, uint64_t sentDataSize) override
    {
        (void)sentDataSize;
    }

private:
    DeviceDelegate* m_delegate;
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> m_peer_connection_factory;

    mediasoupclient::Device m_device {};
    std::unique_ptr<mediasoupclient::SendTransport> m_send_transport { nullptr };
    std::unique_ptr<mediasoupclient::RecvTransport> m_recv_transport { nullptr };

    std::vector<std::unique_ptr<SinkImpl>> m_sinks {};
    std::vector<std::unique_ptr<DataConsumerImpl>> m_data_sinks {};

    std::unordered_map<
        const void*,
        std::variant<
            std::shared_ptr<VideoSenderImpl>,
            std::shared_ptr<AudioSenderImpl>,
            std::shared_ptr<DataSenderImpl>>>
        m_senders {};
};

void DeviceImpl::ensure_transport(TransportKind kind) noexcept
{
    if (kind == TransportKind::Send && m_send_transport)
        return;

    if (kind == TransportKind::Recv && m_recv_transport)
        return;

    auto transport_options = m_delegate->create_server_side_transport(kind, this->rtp_capabilities()).get();

    mediasoupclient::PeerConnection::Options options;
    options.factory = m_peer_connection_factory.get();

    switch (kind) {
    case TransportKind::Send:
        m_send_transport = std::unique_ptr<mediasoupclient::SendTransport>(
            m_device.CreateSendTransport(this,
                transport_options.id,
                transport_options.ice_parameters,
                transport_options.ice_candidates,
                transport_options.dtls_parameters,
                transport_options.sctp_parameters,
                &options));

        break;
    case TransportKind::Recv:
        m_recv_transport = std::unique_ptr<mediasoupclient::RecvTransport>(
            m_device.CreateRecvTransport(this,
                transport_options.id,
                transport_options.ice_parameters,
                transport_options.ice_candidates,
                transport_options.dtls_parameters,
                transport_options.sctp_parameters,
                &options));

        break;
    }
}

void DeviceImpl::create_video_sink(const ConsumerOptions& options, std::shared_ptr<VideoConsumer> user_consumer) noexcept
{
    ensure_transport(TransportKind::Recv);

    auto consumer = std::unique_ptr<mediasoupclient::Consumer>(
        m_recv_transport->Consume(
            this,
            options.consumer_id,
            options.producer_id,
            kVideo,
            options.rtp_parameters.is_null() ? nullptr : const_cast<nlohmann::json*>(&options.rtp_parameters)));

    m_sinks.emplace_back(std::make_unique<VideoSinkImpl>(std::move(consumer), std::move(user_consumer)));
}

void DeviceImpl::create_audio_sink(const ConsumerOptions& options, std::shared_ptr<AudioConsumer> user_consumer) noexcept
{
    ensure_transport(TransportKind::Recv);

    auto consumer = std::unique_ptr<mediasoupclient::Consumer>(
        m_recv_transport->Consume(
            this,
            options.consumer_id,
            options.producer_id,
            kAudio,
            options.rtp_parameters.is_null() ? nullptr : const_cast<nlohmann::json*>(&options.rtp_parameters)));

    m_sinks.emplace_back(std::make_unique<AudioSinkImpl>(std::move(consumer), std::move(user_consumer)));
}

void DeviceImpl::close_sink(const void* consumer) noexcept
{
    for (auto it = m_sinks.begin(); it != m_sinks.end(); it++) {
        auto& sink = *it;
        if (!sink->is_user_ptr_equal(consumer))
            continue;

        sink->on_close();
        m_sinks.erase(it);
        break;
    }
}

void DeviceImpl::OnTransportClose(mediasoupclient::Consumer* consumer)
{
    for (auto it = m_sinks.begin(); it != m_sinks.end(); it++) {
        auto& sink = *it;
        if (!sink->is_consumer_equal(consumer))
            continue;

        sink->on_close();
        m_sinks.erase(it);
        break;
    }
}

void DeviceImpl::close_sender(const void* producer) noexcept
{
    m_senders.erase(producer);
}

std::shared_ptr<VideoSender> DeviceImpl::create_video_source(const ProducerOptions& options) noexcept
{
    ensure_transport(TransportKind::Send);

    auto* source = new rtc::RefCountedObject<::msc::VideoSenderImpl>(2, false);
    auto track = m_peer_connection_factory->CreateVideoTrack("video_track_X", source);

    (void)options;
    // m_send_transport->Produce(this, )

    return nullptr;
}

std::shared_ptr<AudioSender> DeviceImpl::create_audio_source(const ProducerOptions& options) noexcept
{
    ensure_transport(TransportKind::Send);

    auto audio_sender = std::make_shared<AudioSenderImpl>();
    auto audio_source = m_peer_connection_factory->CreateAudioSource(cricket::AudioOptions());
    auto track = m_peer_connection_factory->CreateAudioTrack("audio_track_X", audio_source.get());

    std::vector<webrtc::RtpEncodingParameters> rtc_encodings;
    if (options.encodings.is_array()) {
        rtc_encodings = options.encodings.get<std::vector<webrtc::RtpEncodingParameters>>();
    }

    auto producer = std::unique_ptr<mediasoupclient::Producer>(m_send_transport->Produce(
        this,
        track.get(),
        options.encodings.is_array() ? &rtc_encodings : nullptr,
        options.codecOptions.is_object() ? &options.codecOptions : nullptr,
        options.codec.is_object() ? &options.codec : nullptr,
        nlohmann::json::object()));

    audio_sender->init(std::move(producer), std::move(track));
    m_senders.insert({ producer.get(), audio_sender });

    return audio_sender;
}

void DeviceImpl::create_data_sink(const std::string& consumer_id, const std::string& producer_id, uint16_t stream_id, const std::string& label, const std::string& protocol, std::shared_ptr<DataConsumer> user_consumer) noexcept
{
    ensure_transport(TransportKind::Recv);

    auto wrapper_consumer = std::make_unique<DataConsumerImpl>(user_consumer);
    auto data_consumer = std::unique_ptr<mediasoupclient::DataConsumer>(
        m_recv_transport->ConsumeData(
            wrapper_consumer.get(),
            consumer_id,
            producer_id,
            stream_id,
            label,
            protocol));

    wrapper_consumer->set_consumer(std::move(data_consumer));
    m_data_sinks.push_back(std::move(wrapper_consumer));
}

void DeviceImpl::close_data_sink(const std::shared_ptr<DataConsumer>& user_consumer) noexcept
{
    for (auto it = m_data_sinks.begin(); it != m_data_sinks.end(); ++it) {
        auto& sink = *it;
        if (sink->is_user_ptr_equal(user_consumer.get())) {
            m_data_sinks.erase(it);
            break;
        }
    }
}

std::shared_ptr<DataSender> DeviceImpl::create_data_source(
    const std::string& label,
    const std::string& protocol,
    bool ordered,
    int maxRetransmits,
    int maxPacketLifeTime) noexcept
{
    ensure_transport(TransportKind::Send);
    auto data_producer = std::unique_ptr<mediasoupclient::DataProducer>(
        m_send_transport->ProduceData(
            this,
            label,
            protocol,
            ordered,
            maxRetransmits,
            maxPacketLifeTime,
            nlohmann::json::object()));

    auto data_sender = std::make_shared<DataSenderImpl>(std::move(data_producer));
    m_senders.insert({ data_producer.get(), data_sender });

    return data_sender;
}

struct ReEncodeContext {
    std::unique_ptr<mediasoupclient::Consumer> consumer;
    std::unique_ptr<mediasoupclient::Producer> producer;

    ReEncodeContext(std::unique_ptr<mediasoupclient::Consumer> consumer, std::unique_ptr<mediasoupclient::Producer> producer)
        : consumer(std::move(consumer))
        , producer(std::move(producer))
    {
    }

    ~ReEncodeContext()
    {
        producer->Close();
        consumer->Close();
    }
};

std::shared_ptr<void> DeviceImpl::re_encode(MediaKind kind, const ConsumerOptions& consumer_options, const ProducerOptions& producer_options) noexcept
{
    ensure_transport(TransportKind::Recv);
    ensure_transport(TransportKind::Send);

    auto consumer = std::unique_ptr<mediasoupclient::Consumer>(
        m_recv_transport->Consume(
            this,
            consumer_options.consumer_id,
            consumer_options.producer_id,
            kind == MediaKind::Audio ? kAudio : kVideo,
            const_cast<nlohmann::json*>(&consumer_options.rtp_parameters)));

    std::vector<webrtc::RtpEncodingParameters> rtc_encodings;
    if (producer_options.encodings.is_array()) {
        rtc_encodings = producer_options.encodings.get<std::vector<webrtc::RtpEncodingParameters>>();
    }

    auto producer = std::unique_ptr<mediasoupclient::Producer>(m_send_transport->Produce(
        this,
        consumer->GetTrack(),
        producer_options.encodings.is_array() ? &rtc_encodings : nullptr,
        producer_options.codecOptions.is_object() ? &producer_options.codecOptions : nullptr,
        producer_options.codec.is_object() ? &producer_options.codec : nullptr,
        nlohmann::json::object()));

    return std::make_shared<ReEncodeContext>(std::move(consumer), std::move(producer));
}

}

void initialize()
{
    static std::mutex s_mutex = {};
    static bool s_initialized = false;

    std::lock_guard<std::mutex> lk(s_mutex);
    if (s_initialized)
        return;

    s_initialized = true;
    cm::init_logger("msc.log");
    rtc::InitializeSSL();
    rtc::InitRandom(static_cast<int>(rtc::TimeMillis()));
    // rtc::LogMessage::LogToDebug(rtc::LS_INFO);

    mediasoupclient::Logger::SetLogLevel(mediasoupclient::Logger::LogLevel::LOG_WARN);
    mediasoupclient::Logger::SetHandler(new FFIMediasoupLogHandler());
}

int64_t rtc_timestamp_ms()
{
    return rtc::TimeMillis();
}

std::unique_ptr<Device> Device::create(DeviceDelegate* delegate, std::shared_ptr<PeerConnectionFactoryTuple> peer_connection_factory_tuple) noexcept
{
    initialize();

    if (!peer_connection_factory_tuple)
        peer_connection_factory_tuple = default_peer_connection_factory();

    return std::make_unique<DeviceImpl>(delegate, static_cast<PeerConnectionFactoryTupleImpl*>(peer_connection_factory_tuple.get())->factory());
}

}
