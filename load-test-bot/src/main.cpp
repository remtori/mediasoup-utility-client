#include <chrono>
#include <iostream>

// #include "ftxui/component/captured_mouse.hpp"     // for ftxui
// #include "ftxui/component/component.hpp"          // for Button, Horizontal, Renderer
// #include "ftxui/component/component_base.hpp"     // for ComponentBase
// #include "ftxui/component/screen_interactive.hpp" // for ScreenInteractive
// #include "ftxui/dom/elements.hpp"                 // for separator, gauge, text, Element, operator|, vbox, border

#include <common/http_client.hpp>
#include <fmt/format.h>
#include <msc/msc.hpp>

static const std::string ENDPOINT = "https://portal-mediasoup-dev.service.zingplay.com";

static std::atomic_int64_t s_liveViewerCount = 0;

class Viewer : public msc::DeviceDelegate
    , public std::enable_shared_from_this<Viewer> {
public:
    Viewer(std::shared_ptr<cm::HttpClient> client, std::shared_ptr<cm::Executor> executor)
        : m_client(std::move(client))
        , m_executor(std::move(executor))
    {
    }

    void watch(std::string streamer_id)
    {
        m_streamer_id = std::move(streamer_id);
        m_device = msc::Device::create(m_executor, this->shared_from_this());

        m_executor->push_task([this]() {
            try {
                auto resp = m_client->post(ENDPOINT + "/live/" + m_streamer_id + "/watch", nlohmann::json::object()).get();
                auto watch_response = resp->GetJson();

                if (!watch_response.value("ok", true)) {
                    fmt::println("watch error: {}", watch_response.dump(2));
                    return 1;
                }

                m_device->load(watch_response.at("routerRtpCapabilities"));
                fmt::println("creating recv transport...");
                m_device->ensure_transport(msc::TransportKind::Recv);

                fmt::println("consuming stream");
                resp = m_client->post(ENDPOINT + "/live/" + m_streamer_id + "/consume", { { "rtpCapabilities", m_device->rtp_capabilities() } }).get();

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
                        m_device->create_video_sink(
                            consumer.at("consumerId").get<std::string>(),
                            consumer.at("producerId").get<std::string>(),
                            consumer.at("rtpParameters"),
                            std::make_shared<msc::DummyVideoConsumer>());
                    } else if (type == "audio") {
                        m_device->create_audio_sink(
                            consumer.at("consumerId").get<std::string>(),
                            consumer.at("producerId").get<std::string>(),
                            consumer.at("rtpParameters"),
                            std::make_shared<msc::DummyAudioConsumer>());
                    } else {
                        m_device->create_video_sink(
                            consumer.at("consumerId").get<std::string>(),
                            consumer.at("producerId").get<std::string>(),
                            consumer.at("rtpParameters"),
                            std::make_shared<msc::DummyVideoConsumer>());
                    }
                }

                m_client->post(ENDPOINT + "/live/" + m_streamer_id + "/resume", nlohmann::json::object());
                s_liveViewerCount.fetch_add(1);
            } catch (const std::exception& ex) {
                fmt::println("[{}] Exception: %s", m_streamer_id, ex.what());
            }
        });
    }

private:
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
        fmt::println("[{}] Connection state changed: transport={} connection_state={}", m_streamer_id, transport_id, connection_state);
        if (connection_state == "close") {
            s_liveViewerCount.fetch_sub(1);
        }
    }

private:
    std::shared_ptr<cm::HttpClient> m_client;
    std::shared_ptr<cm::Executor> m_executor;

    std::string m_streamer_id {};

    std::shared_ptr<msc::Device> m_device { nullptr };
    nlohmann::json m_create_transport_option {};
};

int main(int argc, const char** argv)
{
    fmt::println("initializing...");
    msc::initialize();

    std::vector<std::shared_ptr<hv::EventLoopThread>> event_loop_threads;
    for (auto i = 0; i < 4; i++) {
        auto event_loop_thread = std::make_shared<hv::EventLoopThread>();
        event_loop_thread->start();

        event_loop_threads.push_back(event_loop_thread);
    }

    auto client = std::make_shared<cm::HttpClient>(event_loop_threads[0]->loop());
    auto tokenResp = client->get(ENDPOINT + "/stats/sign").get();
    auto tokenJson = tokenResp->GetJson();
    if (!tokenJson.value("ok", false)) {
        fmt::println("Error cannot get token: {}", tokenJson.dump(2));
        return 1;
    }

    client->headers()["Authorization"] = "Bearer " + tokenJson.at("token").get<std::string>();
    auto executor = std::make_shared<cm::Executor>();
    auto viewer = std::make_shared<Viewer>(client, executor);
    viewer->watch("1174393215");

    // auto buttons = ftxui::Container::Vertical({
    //     ftxui::Container::Horizontal({
    //         ftxui::text("Target viewers"),
    //         Button(
    //             "-1", [&] { value--; }, ButtonStyle()),
    //         Button(
    //             "+1", [&] { value++; }, ButtonStyle()),
    //     }) | ftxui::flex,
    // });

    // // Modify the way to render them on screen:
    // auto component = ftxui::Renderer(buttons, [&] {
    //     return ftxui::vbox({
    //                ftxui::text("value = " + std::to_string(s_liveViewerCount.load())),
    //                ftxui::separator(),
    //                buttons->Render() | ftxui::flex,
    //            })
    //         | ftxui::flex | ftxui::border;
    // });

    // auto screen = ftxui::ScreenInteractive::Fullscreen();
    // screen.Loop(component);

    for (auto& event_loop_thread : event_loop_threads) {
        event_loop_thread->stop();
        event_loop_thread->join();
    }
}
