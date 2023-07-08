#include <chrono>

#include <common/http_client.hpp>
#include <fmt/format.h>
#include <msc/msc.hpp>

static const std::string ENDPOINT = "https://portal-mediasoup-dev.service.zingplay.com";
static const std::string BEARER_AUTH = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1aWQiOiI5IiwicmlkIjoiOSIsImlhdCI6MTY4NzI1MzQ5MiwiZXhwIjoxNzg3MjU3MDkyfQ.q099PYyLjLuGC7nWlOuEFPDyKEmgqSrIWwHEqsgLXyc";

class Signaling : public msc::DeviceDelegate {
public:
    Signaling(std::shared_ptr<cm::HttpClient> client, std::string streamer_id)
        : m_client(std::move(client))
        , m_streamer_id(std::move(streamer_id))
    {
    }

    std::future<msc::CreateTransportOptions> create_server_side_transport(msc::TransportKind kind, const nlohmann::json& rtp_capabilities) noexcept override
    {
        fmt::println("Create server side transport {}", static_cast<int>(kind));

        nlohmann::json body = {
            { "rtpCapabilities", rtp_capabilities },
        };

        auto resp = m_client->post(ENDPOINT + "/live/" + m_streamer_id + "/createTransport", body).get();
        auto json = resp->GetJson();

        msc::CreateTransportOptions options;
        json.at("id").get_to(options.id);
        options.ice_parameters = json.value("iceParameters", nullptr);
        options.ice_candidates = json.value("iceCandidates", nullptr);
        options.dtls_parameters = json.value("dtlsParameters", nullptr);
        options.sctp_parameters = json.value("sctpParameters", nullptr);

        std::promise<msc::CreateTransportOptions> promise;
        promise.set_value(options);
        return promise.get_future();
    }

    std::future<void> connect_transport(const std::string& transport_id, const nlohmann::json& dtls_parameters) noexcept override
    {
        fmt::println("Connect transport {}", transport_id);

        nlohmann::json body = {
            { "dtlsParameters", dtls_parameters }
        };

        m_client->post(ENDPOINT + "/live/" + m_streamer_id + "/connectTransport", body).get();

        std::promise<void> ret;
        ret.set_value();
        return ret.get_future();
    }

    std::future<std::string> connect_producer(const std::string& transport_id, msc::MediaKind kind, const nlohmann::json& rtp_parameters) noexcept override
    {
        (void)transport_id;
        (void)kind;
        (void)rtp_parameters;

        std::promise<std::string> promise;
        promise.set_exception(std::make_exception_ptr(std::runtime_error("not implemented")));
        return promise.get_future();
    }

    std::future<std::string> connect_data_producer(const std::string& transport_id, const nlohmann::json& sctp_parameters, const std::string& label, const std::string& protocol) noexcept override
    {
        (void)transport_id;
        (void)sctp_parameters;
        (void)label;
        (void)protocol;

        std::promise<std::string> promise;
        promise.set_exception(std::make_exception_ptr(std::runtime_error("not implemented")));
        return promise.get_future();
    }

    void on_connection_state_change(const std::string& transport_id, const std::string& connection_state) noexcept override
    {
        fmt::println("Connection state changed: transport={} connection_state={}", transport_id, connection_state);
    }

private:
    std::shared_ptr<cm::HttpClient> m_client;
    std::string m_streamer_id;
};

int main()
{
    const std::string streamer_id = "1222655792";

    auto event_loop_thread = std::make_shared<hv::EventLoopThread>();
    event_loop_thread->start();

    auto client = std::make_shared<cm::HttpClient>(event_loop_thread->loop());
    client->headers()["Authorization"] = "Bearer " + BEARER_AUTH;

    auto resp = client->get(ENDPOINT + "/live/" + streamer_id).get();
    auto rtp_capabilities = resp->GetJson();

    fmt::println("rtp_capabilities {}", rtp_capabilities.dump(2));

    auto executor = std::make_shared<cm::Executor>();
    auto device = msc::Device::create(executor, std::make_shared<Signaling>(client, streamer_id));

    device->load(rtp_capabilities);
    device->ensure_transport(msc::TransportKind::Recv);

    resp = client->post(ENDPOINT + "/live/" + streamer_id + "/consume", device->rtp_capabilities()).get();
    auto consumers = resp->GetJson();

    for (auto& consumer : consumers) {
        auto kind = consumer.at("kind").get<std::string>();

        if (kind == "video") {
            device->create_video_sink(consumer.at("consumerId").get<std::string>(), consumer.at("producerId").get<std::string>(), consumer.at("rtpParameters"), nullptr);
        }
    }

    client->post(ENDPOINT + "/live/" + streamer_id + "/resume", nlohmann::json::object());

    event_loop_thread->stop();
    event_loop_thread->join();
}
