#include "conference.hpp"

#include "timer_event_loop.hpp"

static constexpr bool USE_LIVE_SERVER = false;
static const std::string WS_ENDPOINT = USE_LIVE_SERVER ? "ws://portal-voicevideo.service.zingplay.com:12009" : "ws://portal-mediasoup-dev.service.zingplay.com:11905";
static const std::string HTTP_ENDPOINT = USE_LIVE_SERVER ? "https://portal-voicevideo.service.zingplay.com" : "http://portal-mediasoup-dev.service.zingplay.com:11905";

ConferencePeer::ConferencePeer(
    std::shared_ptr<cm::Executor> executor,
    hv::EventLoopPtr event_loop,
    std::shared_ptr<cm::HttpClient> http_client,
    std::shared_ptr<msc::PeerConnectionFactoryTuple> peer_connection_factory)
    : m_executor(std::move(executor))
    , m_event_loop(event_loop)
    , m_protoo(event_loop)
    , m_http_client(http_client)
    , m_peer_connection_factory(std::move(peer_connection_factory))
{
    m_device = msc::Device::create(this, m_peer_connection_factory);
    m_protoo.on_notify = std::bind(&ConferencePeer::on_protoo_notify, this, std::placeholders::_1);
    m_protoo.on_request = std::bind(&ConferencePeer::on_protoo_request, this, std::placeholders::_1);
}

ConferencePeer::~ConferencePeer()
{
    leave(true);
}

void ConferencePeer::joinRoom(std::string user_id, std::string room_id)
{
    m_user_id = std::move(user_id);
    m_room_id = std::move(room_id);

    m_executor->push_task([this]() {
        auto auth_response = m_http_client->get(HTTP_ENDPOINT + "/api/conference/__internalRouteForTestPurpose_REMOVE_IN_PROD?uid=" + m_user_id).get();
        auto auth_json = auth_response->GetJson();

        m_protoo.connect(WS_ENDPOINT + "/conference/connect?rid=" + m_room_id + "&token=" + auth_json.at("data").get<std::string>());

        request("join", {
                            { "roomId", m_room_id },
                            { "deviceId", "BOT1111" },
                            { "deviceModel", "Linux" },
                            { "networkType", "LAN" },
                            { "gameId", "werewolf" },
                            { "cameraResolution", "TODO" },
                        });

        auto routerRtpCapabilities = request("getRouterRtpCapabilities", {});

        m_device->load(routerRtpCapabilities);
        m_create_transport_option = request("createWebRtcTransport", {});
        m_device->ensure_transport(msc::TransportKind::Send);
        m_device->ensure_transport(msc::TransportKind::Recv);

        auto consumer_infos = request("consumeAllExistingProducer", { { "rtpCapabilities", m_device->rtp_capabilities() } });
        start_consuming(consumer_infos);

        m_self_data_sender = m_device->create_data_source("virtual-avatar", "", false, 0, 0);
        m_self_audio_sender = m_device->create_audio_source(
            nullptr,
            {
                { "opusStereo", true },
                { "opusDtx", true },
            },
            nullptr);
    });
}

void ConferencePeer::leave(bool blocking)
{
    auto fut = m_executor->submit([this]() {
        m_protoo.close();
        m_device->stop();
        m_peers.clear();
        m_self_audio_sender.reset();
        m_self_data_sender.reset();
    });

    if (blocking) {
        fut.get();
    }
}

ConferencePeer::Peer& ConferencePeer::get_or_create_peer(const std::string& peer_id)
{
    auto it = m_peers.find(peer_id);
    if (it != m_peers.end()) {
        return it->second;
    }

    auto& peer = m_peers[peer_id];
    return peer;
}

void ConferencePeer::tick_producer()
{
    m_buffer.resize(1760);
    std::generate(m_buffer.begin(), m_buffer.end(), []() { return rand() % 256; });
    if (m_self_data_sender) {
        m_self_data_sender->send_data(std::span(m_buffer.begin(), 300));
    }

    // this will be called every 10ms, so sending 440 frames every 10ms will be 44000Hz
    if (m_self_audio_sender) {
        msc::MutableAudioData audio_data;
        audio_data.timestamp_ms = msc::rtc_timestamp_ms();
        audio_data.bits_per_sample = 16;
        audio_data.sample_rate = 44000;
        audio_data.number_of_channels = 2;
        audio_data.number_of_frames = 440;
        audio_data.data = m_buffer.data();
        m_self_audio_sender->send_audio_data(audio_data);
    }
}

void ConferencePeer::start_consuming(nlohmann::json consumer_infos)
{
    for (const auto& consumer_info : consumer_infos) {
        const auto& peer_id = consumer_info.at("userId").get<std::string>();
        const auto& consumer_id = consumer_info.at("consumerId").get<std::string>();
        const auto& producer_id = consumer_info.at("producerId").get<std::string>();
        const auto& producer_type = consumer_info.at("producerType").get<std::string>();

        auto& peer = get_or_create_peer(peer_id);
        if (producer_type == "data") {
            auto stream_id = consumer_info.at("streamId").get<int16_t>();
            const auto& label = consumer_info.at("label").get<std::string>();
            const auto& protocol = consumer_info.at("protocol").get<std::string>();

            if (!peer.data_consumer) {
                peer.data_consumer = std::make_shared<ReportDataConsumer>();
            }

            m_device->create_data_sink(consumer_id, producer_id, stream_id, label, protocol, peer.data_consumer);
        } else {
            const std::string kind = producer_type == "audio" ? "audio" : "video";
            auto rtp_parameters = consumer_info.at("rtpParameters");

            if (kind == "audio") {
                if (!peer.audio_consumer) {
                    peer.audio_consumer = std::make_shared<msc::DummyAudioConsumer>();
                }

                m_device->create_audio_sink(consumer_id, producer_id, rtp_parameters, peer.audio_consumer);
            } else {
                if (!peer.video_consumer) {
                    peer.video_consumer = std::make_shared<ReportVideoConsumer>();
                }

                m_device->create_video_sink(consumer_id, producer_id, rtp_parameters, peer.video_consumer);
            }
        }
    }
}

void ConferencePeer::on_protoo_notify(cm::ProtooNotify req)
{
    if (req.method == "kick") {

    } else if (req.method == "consumerPaused") {

    } else if (req.method == "consumerResumed") {
    }
}

void ConferencePeer::on_protoo_request(cm::ProtooRequest req)
{
    if (req.method == "newConsumer") {
        start_consuming(nlohmann::json::array({
            {
                { "userId", req.data.at("userId") },
                { "consumerId", req.data.at("consumerId") },
                { "producerId", req.data.at("producerId") },
                { "producerType", req.data.at("kind") },
                { "rtpParameters", req.data.at("rtpParameters") },
                { "producerPaused", req.data.at("producerPaused") },
            },
        }));

        m_protoo.response(req.ok({}));
    } else if (req.method == "newDataConsumer") {
        start_consuming(nlohmann::json::array({
            {
                { "userId", req.data.at("userId") },
                { "consumerId", req.data.at("consumerId") },
                { "producerId", req.data.at("producerId") },
                { "producerType", "data" },
                { "streamId", req.data.at("streamId") },
                { "label", req.data.at("label") },
                { "protocol", req.data.at("protocol") },
            },
        }));

        m_protoo.response(req.ok({}));
    } else {
        m_protoo.response(req.err("not found"));
    }
}

std::future<msc::CreateTransportOptions> ConferencePeer::create_server_side_transport(msc::TransportKind kind, const nlohmann::json& rtp_capabilities) noexcept
{
    (void)rtp_capabilities;
    auto info = m_create_transport_option[kind == msc::TransportKind::Send ? "sendTransport" : "recvTransport"];

    msc::CreateTransportOptions options;
    options.id = info.at("transportId").get<std::string>();
    options.ice_parameters = info.value("iceParameters", nlohmann::json());
    options.ice_candidates = info.value("iceCandidates", nlohmann::json());
    options.dtls_parameters = info.value("dtlsParameters", nlohmann::json());
    options.sctp_parameters = info.value("sctpParameters", nlohmann::json());

    std::promise<msc::CreateTransportOptions> ret;
    ret.set_value(options);
    return ret.get_future();
}

std::future<void> ConferencePeer::connect_transport(msc::TransportKind kind, const std::string& transport_id, const nlohmann::json& dtls_parameters) noexcept
{
    (void)transport_id;
    request("connectWebRtcTransport", {
                                          { "isSend", kind == msc::TransportKind::Send },
                                          { "dtlsParameters", dtls_parameters },
                                      });

    std::promise<void> ret;
    ret.set_value();
    return ret.get_future();
}

std::future<std::string> ConferencePeer::connect_producer(const std::string& transport_id, msc::MediaKind kind, const nlohmann::json& rtp_parameters) noexcept
{
    (void)transport_id;
    auto resp = request("produce", {
                                       { "kind", kind == msc::MediaKind::Audio ? "audio" : "video" },
                                       { "rtpParameters", rtp_parameters },
                                   });

    std::promise<std::string> ret;
    ret.set_value(resp.at("producerId").get<std::string>());
    return ret.get_future();
}

std::future<std::string> ConferencePeer::connect_data_producer(const std::string& transport_id, const nlohmann::json& sctp_parameters, const std::string& label, const std::string& protocol) noexcept
{
    (void)transport_id;
    auto resp = request("produceData", {
                                           { "label", label },
                                           { "protocol", protocol },
                                           { "sctpStreamParameters", sctp_parameters },
                                       });

    std::promise<std::string> ret;
    ret.set_value(resp.at("producerId").get<std::string>());
    return ret.get_future();
}

void ConferencePeer::on_connection_state_change(msc::TransportKind, const std::string&, const std::string& connection_state) noexcept
{
    (void)connection_state;
}
