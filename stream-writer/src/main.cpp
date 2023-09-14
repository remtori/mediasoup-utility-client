#include <chrono>
#include <iostream>

#include <common/http_client.hpp>
#include <fmt/format.h>
#include <msc/msc.hpp>

#include "./video-writer.hpp"

static const std::string ENDPOINT = "https://portal-mediasoup-dev.service.zingplay.com";
static const std::string BEARER_AUTH = "eyJhbGciOiJSUzI1NiJ9.eyJzdWIiOiIxMTc0MzkzMjE1IiwiaWF0IjoxNjk4MDc3MzI5LCJleHAiOjE2OTg2ODIxMjl9.sJ0o8jC7PREfP98AWqAeMNAf_fxXRyiZ1TcTZ12vhpNk7q5yy0Wfr1OKPZZaClboLBqA-2_wbnkdkLlMBL7vtFeraTvkszYQGLppAPWMOx0Haxe90mUCFp37AP9U8V4zK-iZ3D3XjpMA5yHBj2kQSLLUzx9gQS7hwc3mChnSc3cMcM1SIw5mmn5uPWHpb0HYrG08Bud3XdlAH85zxf4A4k1OkGBkpyUdDXTa0h0dvphBMzlFywVMfXswrLW417O-MpfNnVjC7d_gwsDOKb5ZMNAyGJpQR9V6HEO1ffWYk5RzBqY_dICrp0jK1aTGd4qpKC7UmNwo3VQxUFSpiE8SFg";

class Signaling : public msc::DeviceDelegate {
public:
    Signaling(std::shared_ptr<cm::HttpClient> client, std::string streamer_id, nlohmann::json create_transport_option)
        : m_client(std::move(client))
        , m_streamer_id(std::move(streamer_id))
        , m_create_transport_option(std::move(create_transport_option))
    {
    }

    std::future<msc::CreateTransportOptions> create_server_side_transport(msc::TransportKind kind, const nlohmann::json& rtp_capabilities) noexcept override
    {
        fmt::println("Create client side transport {}", static_cast<int>(kind));
        (void)rtp_capabilities;

        msc::CreateTransportOptions options;
        options.id = m_create_transport_option.at("transportId").get<std::string>();
        options.ice_parameters = m_create_transport_option.value("iceParameters", nlohmann::json());
        options.ice_candidates = m_create_transport_option.value("iceCandidates", nlohmann::json());
        options.dtls_parameters = m_create_transport_option.value("dtlsParameters", nlohmann::json());
        options.sctp_parameters = m_create_transport_option.value("sctpParameters", nlohmann::json());

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
    nlohmann::json m_create_transport_option;
};

int main(int argc, const char** argv)
{
    const std::string streamer_id = argc > 1 ? argv[1] : "1222655792";
    fmt::println("started watching {}", streamer_id);

    auto event_loop_thread = std::make_shared<hv::EventLoopThread>();
    event_loop_thread->start();

    auto client = std::make_shared<cm::HttpClient>(event_loop_thread->loop());
    client->headers()["Authorization"] = "Bearer " + BEARER_AUTH;

    auto resp = client->post(ENDPOINT + "/live/" + streamer_id + "/watch", nlohmann::json::object()).get();
    auto watch_response = resp->GetJson();

    if (!watch_response.value("ok", true)) {
        fmt::println("watch error: {}", watch_response.dump(2));
        return 1;
    }

    auto executor = std::make_shared<cm::Executor>();
    auto device = msc::Device::create(executor, std::make_shared<Signaling>(client, streamer_id, watch_response));

    device->load(watch_response.at("routerRtpCapabilities"));
    fmt::println("creating recv transport...");
    device->ensure_transport(msc::TransportKind::Recv);

    fmt::println("consuming stream");
    resp = client->post(ENDPOINT + "/live/" + streamer_id + "/consume", { { "rtpCapabilities", device->rtp_capabilities() } }).get();
    auto consume_response = resp->GetJson();
    if (!consume_response.value("ok", true)) {
        fmt::println("consume error got: {}", consume_response.dump(2));
        return 1;
    }

    for (auto& consumer : consume_response.at("data")) {
        if (!consumer.value("ok", true)) {
            continue;
        }

        auto type = consumer.at("producerType").get<std::string>();

        fmt::println("consumer: type={}", type);
        if (type == "screen") {
            fmt::println("got video screen");
            device->create_video_sink(
                consumer.at("consumerId").get<std::string>(),
                consumer.at("producerId").get<std::string>(),
                consumer.at("rtpParameters"),
                std::make_shared<VideoWriter>(std::string("output/stream-") + streamer_id + ".mkv"));
        }
    }

    fmt::println("resuming..");
    client->post(ENDPOINT + "/live/" + streamer_id + "/resume", nlohmann::json::object());

    fmt::println("Press ENTER to stop!");
    std::cin.get();

    device.reset();
    executor.reset();

    event_loop_thread->stop();
    event_loop_thread->join();
}
