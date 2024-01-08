#pragma once

#include <common/executor.hpp>
#include <common/http_client.hpp>
#include <common/logger.hpp>
#include <common/protoo.hpp>
#include <msc/msc.hpp>

#include "./consumer.hpp"

enum class ConferenceStatus {
    Idle,
    Handshaking,
    Exception,
    New,
    Checking,
    Connected,
    Completed,
    Failed,
    Disconnected,
    Closed,
};

struct ConferenceState {
    ConferenceStatus status { ConferenceStatus::Idle };
    uint32_t peer_count { 0 };
    uint32_t data_consumer_count { 0 };
    uint32_t audio_consumer_count { 0 };
    bool produce_data { false };
    bool produce_audio { false };
};

class ConferencePeer : public msc::DeviceDelegate
    , public std::enable_shared_from_this<ConferencePeer> {
private:
    struct Peer {
        std::shared_ptr<ReportVideoConsumer> video_consumer { nullptr };
        std::shared_ptr<msc::DummyAudioConsumer> audio_consumer { nullptr };
        std::shared_ptr<ReportDataConsumer> data_consumer { nullptr };
    };

    std::shared_ptr<cm::Executor> m_executor;
    hv::EventLoopPtr m_event_loop;
    cm::ProtooClient m_protoo;
    std::shared_ptr<cm::HttpClient> m_http_client;
    std::shared_ptr<msc::PeerConnectionFactoryTuple> m_peer_connection_factory;

    std::shared_ptr<msc::Device> m_device { nullptr };
    nlohmann::json m_create_transport_option {};

    std::string m_user_id {};
    std::string m_room_id {};

    ConferenceState m_state {};

    std::vector<uint8_t> m_buffer {};
    std::shared_ptr<msc::DataSender> m_self_data_sender {};
    std::shared_ptr<msc::AudioSender> m_self_audio_sender {};
    std::unordered_map<std::string, Peer> m_peers {};

public:
    ConferencePeer(std::shared_ptr<cm::Executor>, hv::EventLoopPtr, std::shared_ptr<cm::HttpClient>, std::shared_ptr<msc::PeerConnectionFactoryTuple>);
    ~ConferencePeer() override;

    void joinRoom(std::string user_id, std::string room_id);
    void leave(bool blocking = false);

    void tick_producer();

private:
    void on_protoo_notify(cm::ProtooNotify);
    void on_protoo_request(cm::ProtooRequest);
    void start_consuming(nlohmann::json consumerInfos);

    inline nlohmann::json request(std::string method, nlohmann::json body)
    {
        auto resp = m_protoo.request(method, std::move(body)).get();
        if (resp.ok) {
            return resp.data;
        }

        throw std::runtime_error(resp.error_reason);
    }

    std::future<msc::CreateTransportOptions> create_server_side_transport(msc::TransportKind kind, const nlohmann::json& rtp_capabilities) noexcept override;
    std::future<void> connect_transport(msc::TransportKind, const std::string& transport_id, const nlohmann::json& dtls_parameters) noexcept override;

    std::future<std::string> connect_producer(const std::string& transport_id, msc::MediaKind kind, const nlohmann::json& rtp_parameters) noexcept override;
    std::future<std::string> connect_data_producer(const std::string& transport_id, const nlohmann::json& sctp_parameters, const std::string& label, const std::string& protocol) noexcept override;

    void on_connection_state_change(msc::TransportKind, const std::string&, const std::string& connection_state) noexcept override;
};
