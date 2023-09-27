#pragma once

#include <common/http_client.hpp>
#include <msc/msc.hpp>

#include "consumer.hpp"

enum class ViewerState {
    Idle,
    Handshaking,
    CreatingTransport,
    Consuming,
    GettingAuthTokenFailed,
    StreamNotFound,
    ConsumeStreamFailed,
    Exception,
    New,
    Checking,
    Connected,
    Completed,
    Failed,
    Disconnected,
    Closed,
};

class Viewer : public msc::DeviceDelegate
    , public std::enable_shared_from_this<Viewer> {
public:
    explicit Viewer(std::shared_ptr<hv::AsyncHttpClient> http_client);
    ~Viewer() override;

    VideoStat video_stat()
    {
        return m_screen_consumer->video_stat();
    }

    ViewerState state() const
    {
        return m_state;
    }

    void watch(std::string streamer_id);

private:
    void stop();

    std::future<msc::CreateTransportOptions> create_server_side_transport(msc::TransportKind kind, const nlohmann::json& rtp_capabilities) noexcept override;
    std::future<void> connect_transport(const std::string& transport_id, const nlohmann::json& dtls_parameters) noexcept override;

    std::future<std::string> connect_producer(const std::string& transport_id, msc::MediaKind kind, const nlohmann::json& rtp_parameters) noexcept override;
    std::future<std::string> connect_data_producer(const std::string& transport_id, const nlohmann::json& sctp_parameters, const std::string& label, const std::string& protocol) noexcept override;

    void on_connection_state_change(const std::string&, const std::string& connection_state) noexcept override;

private:
    cm::HttpClient m_client;
    std::shared_ptr<ReportVideoConsumer> m_screen_consumer;

    std::string m_streamer_id {};
    ViewerState m_state { ViewerState::Idle };

    std::shared_ptr<msc::Device> m_device { nullptr };
    nlohmann::json m_create_transport_option {};

    std::string m_session_key {};
    hv::TimerID m_ping_interval {};
    bool m_stopped { true };
};