#include "msc/msc.hpp"

#include "./assert.hpp"
#include "./common.hpp"
#include "./media_sink.hpp"

#include <MediaSoupClientErrors.hpp>
#include <mediasoupclient.hpp>

#include <rtc_base/byte_order.h>
#include <rtc_base/ssl_adapter.h>
#include <system_wrappers/include/clock.h>

namespace msc {

webrtc::PeerConnectionFactoryInterface* peer_connection_factory();

namespace {

const std::string kAudio = "audio";
const std::string kVideo = "video";

class FFIMediasoupLogHandler : public mediasoupclient::Logger::LogHandlerInterface {
public:
    void OnLog(mediasoupclient::Logger::LogLevel level, char* payload, size_t len) override
    {
        println("[MS]({}): {}", static_cast<int>(level), std::string_view(payload, len));
    }
};

static void initialize()
{
    static std::mutex s_mutex = {};
    static bool s_initialized = false;

    std::lock_guard<std::mutex> lk(s_mutex);
    if (s_initialized)
        return;

    s_initialized = true;
    rtc::InitializeSSL();
    rtc::InitRandom(static_cast<int>(rtc::TimeMillis()));
    // rtc::LogMessage::LogToDebug(rtc::LS_INFO);
    // rtc::LogMessage::AddLogToStream(new impl::FFIWebrtcLogSink(), rtc::LS_INFO);

    mediasoupclient::Logger::SetLogLevel(mediasoupclient::Logger::LogLevel::LOG_WARN);
    mediasoupclient::Logger::SetHandler(new FFIMediasoupLogHandler());

    peer_connection_factory();
}

class DeviceImpl : public Device
    , public mediasoupclient::SendTransport::Listener
    , public mediasoupclient::RecvTransport::Listener
    , public mediasoupclient::Producer::Listener
    , public mediasoupclient::Consumer::Listener {
public:
    DeviceImpl(std::shared_ptr<Executor> executor, std::shared_ptr<DeviceDelegate> delegate)
        : m_executor(std::move(executor))
        , m_delegate(std::move(delegate))
    {
    }

    bool load(const nlohmann::json& rtp_capabilities) noexcept override
    {
        if (m_device.IsLoaded())
            return false;

        m_device.Load(rtp_capabilities);
        return true;
    }

    const nlohmann::json& rtp_capabilities() const noexcept override { return m_device.GetRtpCapabilities(); }

    bool can_produce(MediaKind kind) noexcept override { return m_device.CanProduce(kind == MediaKind::Audio ? kAudio : kAudio); }

    void ensure_transport(TransportKind kind) noexcept override { (void)get_or_create_transport(kind); }

    void create_video_sink(const std::string& id, const std::string& producer_id, const nlohmann::json& rtp_parameters, std::shared_ptr<VideoConsumer> consumer) noexcept override;
    void create_audio_sink(const std::string& id, const std::string& producer_id, const nlohmann::json& rtp_parameters, std::shared_ptr<AudioConsumer> consumer) noexcept override;

    void close_video_sink(const std::shared_ptr<VideoConsumer>& consumer) noexcept override { close_sink(consumer.get()); }
    void close_audio_sink(const std::shared_ptr<AudioConsumer>& consumer) noexcept override { close_sink(consumer.get()); }

private:
    const mediasoupclient::Transport& get_or_create_transport(TransportKind kind);
    void close_sink(const void* consumer) noexcept;

public:
    std::future<void> OnConnect(mediasoupclient::Transport* transport, const nlohmann::json& dtls_parameters) override
    {
        return m_delegate->connect_transport(transport->GetId(), dtls_parameters);
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

    void OnTransportClose(mediasoupclient::Producer* producer) override
    {
        (void)producer;
    }

    void OnTransportClose(mediasoupclient::Consumer* consumer) override;

    void OnConnectionStateChange(mediasoupclient::Transport* transport, const std::string& state) override
    {
        m_executor->push_task(&DeviceDelegate::on_connection_state_change, m_delegate, transport->GetId(), state);
    }

private:
    std::shared_ptr<Executor> m_executor;
    std::shared_ptr<DeviceDelegate> m_delegate;

    mediasoupclient::Device m_device {};
    std::unique_ptr<mediasoupclient::SendTransport> m_send_transport {};
    std::unique_ptr<mediasoupclient::RecvTransport> m_recv_transport {};

    std::vector<std::unique_ptr<SinkImpl>> m_sinks {};
};

const mediasoupclient::Transport& DeviceImpl::get_or_create_transport(TransportKind kind)
{
    ASSERT_SAME_THREAD(m_executor->thread_id());

    if (kind == TransportKind::Send && m_send_transport)
        return *m_send_transport;

    if (kind == TransportKind::Recv && m_recv_transport)
        return *m_recv_transport;

    auto transport_options = m_delegate->create_server_side_transport(kind).get();

    mediasoupclient::PeerConnection::Options options;
    options.factory = peer_connection_factory();

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

        return *m_send_transport;
    case TransportKind::Recv:
        m_recv_transport = std::unique_ptr<mediasoupclient::RecvTransport>(
            m_device.CreateRecvTransport(this,
                transport_options.id,
                transport_options.ice_parameters,
                transport_options.ice_candidates,
                transport_options.dtls_parameters,
                transport_options.sctp_parameters,
                &options));

        return *m_recv_transport;
    }
}

void DeviceImpl::create_video_sink(const std::string& id, const std::string& producer_id, const nlohmann::json& rtp_parameters, std::shared_ptr<VideoConsumer> user_consumer) noexcept
{
    ASSERT_SAME_THREAD(m_executor->thread_id());

    auto consumer = std::unique_ptr<mediasoupclient::Consumer>(
        m_recv_transport->Consume(
            this,
            id,
            producer_id,
            kVideo,
            rtp_parameters.is_null() ? nullptr : const_cast<nlohmann::json*>(&rtp_parameters)));

    m_sinks.emplace_back(std::make_unique<VideoSinkImpl>(std::move(consumer), std::move(user_consumer)));
}

void DeviceImpl::create_audio_sink(const std::string& id, const std::string& producer_id, const nlohmann::json& rtp_parameters, std::shared_ptr<AudioConsumer> user_consumer) noexcept
{
    ASSERT_SAME_THREAD(m_executor->thread_id());

    auto consumer = std::unique_ptr<mediasoupclient::Consumer>(
        m_recv_transport->Consume(
            this,
            id,
            producer_id,
            kAudio,
            rtp_parameters.is_null() ? nullptr : const_cast<nlohmann::json*>(&rtp_parameters)));

    m_sinks.emplace_back(std::make_unique<AudioSinkImpl>(std::move(consumer), std::move(user_consumer)));
}

void DeviceImpl::close_sink(const void* consumer) noexcept
{
    ASSERT_SAME_THREAD(m_executor->thread_id());

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
    m_executor->push_task([&] {
        for (auto it = m_sinks.begin(); it != m_sinks.end(); it++) {
            auto& sink = *it;
            if (!sink->is_consumer_equal(consumer))
                continue;

            sink->on_close();
            m_sinks.erase(it);
            break;
        }
    });
}

}

std::shared_ptr<Device> Device::create(std::shared_ptr<Executor> executor, std::shared_ptr<DeviceDelegate> delegate) noexcept
{
    initialize();
    return std::make_shared<DeviceImpl>(std::move(executor), std::move(delegate));
}

}