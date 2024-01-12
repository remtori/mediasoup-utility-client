#include "conference.hpp"

#include "timer_event_loop.hpp"

static constexpr bool USE_LIVE_SERVER = true;
static const std::string WS_ENDPOINT = USE_LIVE_SERVER ? "ws://45.127.252.204:12009" : "ws://portal-mediasoup-dev.service.zingplay.com:11905";
static const std::string HTTP_ENDPOINT = USE_LIVE_SERVER ? "http://45.127.252.204:12009" : "http://portal-mediasoup-dev.service.zingplay.com:11905";

ConferencePeer::ConferencePeer(
    std::shared_ptr<cm::Executor> executor,
    hv::EventLoopPtr event_loop,
    std::shared_ptr<net::HttpClient> http_client,
    std::shared_ptr<msc::PeerConnectionFactoryTuple> peer_connection_factory)
    : m_executor(std::move(executor))
    , m_event_loop(event_loop)
    , m_protoo(event_loop)
    , m_http_client(http_client)
    , m_peer_connection_factory(std::move(peer_connection_factory))
{
    m_device = msc::Device::create(this, m_peer_connection_factory);
    m_protoo.on_notify = [this](net::ProtooNotify req) {
        m_executor->push_task([this, req = std::move(req)]() {
            on_protoo_notify(std::move(req));
        });
    };
    m_protoo.on_request = [this](net::ProtooRequest req) {
        m_executor->push_task([this, req = std::move(req)]() {
            on_protoo_request(std::move(req));
        });
    };
}

ConferencePeer::~ConferencePeer()
{
    leave(true);
}

void ConferencePeer::joinRoom(std::string user_id, std::string room_id)
{
    m_user_id = std::move(user_id);
    m_room_id = std::move(room_id);
    m_state.produce_success = false;

    m_executor->push_task([this]() {
        try {
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

            m_self_audio_sender = m_device->create_audio_source(msc::ProducerOptions {
                .encodings = nullptr,
                .codecOptions = {
                    { "opusStereo", true },
                    { "opusDtx", true },
                },
                .codec = nullptr });

            m_self_data_sender = m_device->create_data_source("virtual-avatar", "", false, 0, 0);
            m_state.produce_success = true;
        } catch (...) {
            m_state.status = ConferenceStatus::Exception;
            std::rethrow_exception(std::current_exception());
        }
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
        m_state.status = ConferenceStatus::Idle;
    });

    if (blocking) {
        fut.get();
    }
}

float ConferencePeer::avg_frame_rate()
{
    float sum_frame_rate = 0;
    for (auto& [_, peer] : m_peers) {
        if (peer.data_consumer) {
            sum_frame_rate += peer.data_consumer->data_stat().frame_rate;
        }
    }

    return sum_frame_rate / m_peers.size();
}

ConferenceState ConferencePeer::state()
{
    ConferenceState ret = m_state;
    m_state.data_producer_tick_count = 0;
    return ret;
}

void ConferencePeer::tick_producer()
{
    m_executor->push_task([this]() {
        {
            m_buffer.resize(1760);
            std::generate(m_buffer.begin(), m_buffer.end(), []() { return std::rand() % 256; });
        }

        if (m_self_data_sender && m_self_data_sender->buffered_amount() == 0) {
            std::span data(m_buffer.begin(), 300);
            if (m_validate_data_channel) {
                cm::CRC32 crc32;
                crc32.update(data.subspan(4));
                uint32_t checksum = crc32.digest();
                std::memcpy(data.data(), &checksum, sizeof(checksum));
            }

            m_state.data_producer_tick_count++;
            m_self_data_sender->send_data(data);
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
    });
}

void ConferencePeer::start_consuming(nlohmann::json consumer_infos)
{
    for (const auto& consumer_info : consumer_infos) {
        const auto& peer_id = consumer_info.at("userId").get<std::string>();
        const auto& consumer_id = consumer_info.at("consumerId").get<std::string>();
        const auto& producer_id = consumer_info.at("producerId").get<std::string>();
        const auto& producer_type = consumer_info.at("producerType").get<std::string>();

        auto& peer = m_peers[peer_id];
        if (producer_type == "data") {
            auto stream_id = consumer_info.at("streamId").get<int16_t>();
            const auto& label = consumer_info.at("label").get<std::string>();
            const auto& protocol = consumer_info.at("protocol").get<std::string>();

            cm::log("[Conference][{}] start consuming data from {}: consumer_id={} producer_id={} stream_id={} label={}", m_user_id, peer_id, consumer_id, producer_id, stream_id, label);
            if (!peer.data_consumer) {
                peer.data_consumer = std::make_shared<ReportDataConsumer>(m_validate_data_channel);
            }

            m_device->create_data_sink(consumer_id, producer_id, stream_id, label, protocol, peer.data_consumer);
        } else {
            const std::string kind = producer_type == "audio" ? "audio" : "video";
            auto rtp_parameters = consumer_info.at("rtpParameters");

            cm::log("[Conference][{}] start consuming {} from {}: consumer_id={} producer_id={}", m_user_id, kind, peer_id, consumer_id, producer_id);
            if (kind == "audio") {
                if (!peer.audio_consumer) {
                    peer.audio_consumer = std::make_shared<msc::DummyAudioConsumer>();
                }

                m_device->create_audio_sink(msc::ConsumerOptions {
                                                .consumer_id = consumer_id,
                                                .producer_id = producer_id,
                                                .rtp_parameters = rtp_parameters },
                    peer.audio_consumer);
            } else {
                if (!peer.video_consumer) {
                    peer.video_consumer = std::make_shared<ReportVideoConsumer>();
                }

                m_device->create_video_sink(msc::ConsumerOptions {
                                                .consumer_id = consumer_id,
                                                .producer_id = producer_id,
                                                .rtp_parameters = rtp_parameters },
                    peer.video_consumer);
            }
        }
    }

    m_state.peer_count = m_peers.size();
}

void ConferencePeer::on_protoo_notify(net::ProtooNotify req)
{
    if (req.method == "kick") {

    } else if (req.method == "consumerPaused") {

    } else if (req.method == "consumerResumed") {
    }
}

void ConferencePeer::on_protoo_request(net::ProtooRequest req)
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
    if (connection_state == "new")
        m_state.status = ConferenceStatus::New;
    else if (connection_state == "checking")
        m_state.status = ConferenceStatus::Checking;
    else if (connection_state == "connected")
        m_state.status = ConferenceStatus::Connected;
    else if (connection_state == "completed")
        m_state.status = ConferenceStatus::Completed;
    else if (connection_state == "failed")
        m_state.status = ConferenceStatus::Failed;
    else if (connection_state == "disconnected")
        m_state.status = ConferenceStatus::Disconnected;
    else if (connection_state == "closed")
        m_state.status = ConferenceStatus::Closed;

    if (m_state.status == ConferenceStatus::Closed || m_state.status == ConferenceStatus::Disconnected || m_state.status == ConferenceStatus::Failed) {
        leave();
    }
}
