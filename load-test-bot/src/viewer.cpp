#include "viewer.hpp"

#include <common/logger.hpp>

#include "timer_event_loop.hpp"

static constexpr bool USE_LIVE_SERVER = false;
static const std::string ENDPOINT = USE_LIVE_SERVER ? "https://portal-voicevideo.service.zingplay.com" : "https://portal-mediasoup-dev.service.zingplay.com";

Viewer::Viewer(std::shared_ptr<hv::AsyncHttpClient> http_client, std::shared_ptr<msc::PeerConnectionFactoryTuple> peer_connection_factory)
    : m_client(std::move(http_client))
    , m_peer_connection_factory(std::move(peer_connection_factory))
    , m_screen_consumer(std::make_shared<ReportVideoConsumer>())
{
}

Viewer::~Viewer()
{
    stop();
}

void Viewer::watch(std::string streamer_id)
{
    m_streamer_id = std::move(streamer_id);
    if (!m_device)
        m_device = msc::Device::create(this, m_peer_connection_factory);
    else
        stop();

    m_stopped = false;
    m_state = ViewerState::Handshaking;

    try {
        if (m_session_key.empty()) {
            auto tokenResp = m_client.get(ENDPOINT + "/stats/sign").get();
            auto tokenJson = tokenResp->GetJson();
            if (!tokenJson.value("ok", false)) {
                cm::log("Error cannot get token: {}", tokenJson.dump(2));
                m_state = ViewerState::GettingAuthTokenFailed;
                return;
            }

            m_client.headers()["Authorization"] = "Bearer " + tokenJson.at("token").get<std::string>();
        }

        auto resp = m_client.post(ENDPOINT + "/live/" + m_streamer_id + "/watch", nlohmann::json::object()).get();
        auto watch_response = resp->GetJson();

        if (!watch_response.value("ok", true)) {
            cm::log("Watch error: {}", watch_response.dump(2));
            m_state = ViewerState::StreamNotFound;
            return;
        }

        m_state = ViewerState::CreatingTransport;
        m_device->load(watch_response.at("routerRtpCapabilities"));
        m_create_transport_option = watch_response;
        m_device->ensure_transport(msc::TransportKind::Recv);

        m_state = ViewerState::Consuming;
        resp = m_client.post(ENDPOINT + "/live/" + m_streamer_id + "/consume", { { "rtpCapabilities", m_device->rtp_capabilities() } }).get();

        auto consume_response = resp->GetJson();
        if (!consume_response.value("ok", true)) {
            cm::log("Consume error got: {}", consume_response.dump(2));
            m_state = ViewerState::ConsumeStreamFailed;
            return;
        }

        for (auto& consumer : consume_response.at("data")) {
            if (!consumer.value("ok", true)) {
                continue;
            }

            auto type = consumer.at("producerType").get<std::string>();

            if (type == "screen") {
                m_screen_consumer->reset();

                m_device->create_video_sink(
                    consumer.at("consumerId").get<std::string>(),
                    consumer.at("producerId").get<std::string>(),
                    consumer.at("rtpParameters"),
                    m_screen_consumer);
            } else if (type == "audio") {
                m_device->create_audio_sink(
                    consumer.at("consumerId").get<std::string>(),
                    consumer.at("producerId").get<std::string>(),
                    consumer.at("rtpParameters"));
            } else {
                m_device->create_video_sink(
                    consumer.at("consumerId").get<std::string>(),
                    consumer.at("producerId").get<std::string>(),
                    consumer.at("rtpParameters"));
            }
        }

        m_client.post(ENDPOINT + "/live/" + m_streamer_id + "/resume", nlohmann::json::object());
        m_ping_interval = timer_event_loop().setInterval(3000, [this](auto) {
            m_client.getAsync(ENDPOINT + "/live/ping", nullptr);
        });
    } catch (const std::exception& ex) {
        cm::log("Exception: {}", ex.what());
        m_state = ViewerState::Exception;
    }
}

void Viewer::stop()
{
    if (m_stopped)
        return;

    m_stopped = true;

    timer_event_loop().killTimer(m_ping_interval);
    m_device->stop();
}

std::future<msc::CreateTransportOptions> Viewer::create_server_side_transport(msc::TransportKind kind, const nlohmann::json& rtp_capabilities) noexcept
{
    (void)rtp_capabilities;

    msc::CreateTransportOptions options;
    options.id = m_create_transport_option.at("transportId").get<std::string>();
    options.ice_parameters = m_create_transport_option.value("iceParameters", nlohmann::json());
    options.ice_candidates = m_create_transport_option.value("iceCandidates", nlohmann::json());
    options.dtls_parameters = m_create_transport_option.value("dtlsParameters", nlohmann::json());
    options.sctp_parameters = m_create_transport_option.value("sctpParameters", nlohmann::json());

    std::promise<msc::CreateTransportOptions> ret;
    ret.set_value(options);
    return ret.get_future();
}

std::future<void> Viewer::connect_transport(msc::TransportKind, const std::string& transport_id, const nlohmann::json& dtls_parameters) noexcept
{
    nlohmann::json body = {
        { "dtlsParameters", dtls_parameters }
    };

    m_client.post(ENDPOINT + "/live/" + m_streamer_id + "/connectTransport", body).get();

    std::promise<void> ret;
    ret.set_value();
    return ret.get_future();
}

std::future<std::string> Viewer::connect_producer(const std::string& transport_id, msc::MediaKind kind, const nlohmann::json& rtp_parameters) noexcept
{
    (void)transport_id;
    (void)kind;
    (void)rtp_parameters;

    std::promise<std::string> promise;
    promise.set_exception(std::make_exception_ptr(std::runtime_error("not implemented")));
    return promise.get_future();
}

std::future<std::string> Viewer::connect_data_producer(const std::string& transport_id, const nlohmann::json& sctp_parameters, const std::string& label, const std::string& protocol) noexcept
{
    (void)transport_id;
    (void)sctp_parameters;
    (void)label;
    (void)protocol;

    std::promise<std::string> promise;
    promise.set_exception(std::make_exception_ptr(std::runtime_error("not implemented")));
    return promise.get_future();
}

void Viewer::on_connection_state_change(msc::TransportKind, const std::string&, const std::string& connection_state) noexcept
{
    if (connection_state == "new")
        m_state = ViewerState::New;
    else if (connection_state == "checking")
        m_state = ViewerState::Checking;
    else if (connection_state == "connected")
        m_state = ViewerState::Connected;
    else if (connection_state == "completed")
        m_state = ViewerState::Completed;
    else if (connection_state == "failed")
        m_state = ViewerState::Failed;
    else if (connection_state == "disconnected")
        m_state = ViewerState::Disconnected;
    else if (connection_state == "closed")
        m_state = ViewerState::Closed;

    if (connection_state == "closed" || connection_state == "disconnected" || connection_state == "failed") {
        stop();
    }
}
