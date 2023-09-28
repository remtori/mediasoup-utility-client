#include <chrono>
#include <iostream>

#include <ftxui/component/captured_mouse.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/flexbox_config.hpp>

#include <common/http_client.hpp>
#include <fmt/format.h>
#include <msc/msc.hpp>
#include <utility>

#include "viewer.hpp"

struct VideoStats {
    float avgFps;
    std::unordered_map<std::string, int> resolution;
};

class ViewerManager {
public:
    explicit ViewerManager(size_t num_worker_thread, size_t num_network_thread, size_t num_peer_connection_factory)
        : m_executor(std::make_unique<cm::Executor>(num_worker_thread))
    {
        m_http_clients.reserve(num_network_thread);
        for (auto i = 0; i < num_network_thread; i++) {
            m_http_clients.push_back(std::make_shared<hv::AsyncHttpClient>());
        }

        m_peer_connection_factories.reserve(num_peer_connection_factory);
        for (auto i = 0; i < num_peer_connection_factory; i++) {
            m_peer_connection_factories.push_back(msc::create_peer_connection_factory());
        }
    }

    void set_streamer_id(std::string streamer_id)
    {
        m_streamer_id = std::move(streamer_id);
    }

    void set_viewer_count(size_t viewer_count)
    {
        if (m_viewers.size() == viewer_count)
            return;

        if (viewer_count < m_viewers.size()) {
            m_viewers.resize(viewer_count);
        } else {
            auto new_viewer_count = viewer_count - m_viewers.size();

            for (auto i = 0; i < new_viewer_count; i++) {
                std::shared_ptr<hv::AsyncHttpClient> http_client = m_http_clients[m_viewers.size() % m_http_clients.size()];
                std::shared_ptr<msc::PeerConnectionFactoryTuple> pc = m_peer_connection_factories[m_viewers.size() % m_peer_connection_factories.size()];
                auto viewer = std::make_shared<Viewer>(http_client, pc);

                m_viewers.push_back(viewer);
                m_executor->push_task([this, viewer]() {
                    viewer->watch(m_streamer_id);
                });
            }
        }
    }

    size_t viewer_count() const
    {
        return m_viewers.size();
    }

    std::unordered_map<ViewerState, int> state_stats()
    {
        std::unordered_map<ViewerState, int> stats {};
        for (const auto& viewer : m_viewers) {
            stats[viewer->state()] += 1;
        }

        return stats;
    }

    VideoStats video_stats()
    {
        VideoStats stats {};
        float sumFps = 0;
        for (const auto& viewer : m_viewers) {
            auto video = viewer->video_stat();

            sumFps += video.frame_rate;
            stats.resolution[fmt::format("{}x{}", video.width, video.height)] += 1;
        }

        stats.avgFps = sumFps / float(m_viewers.size());
        return stats;
    }
private:
    std::unique_ptr<cm::Executor> m_executor;
    std::vector<std::shared_ptr<hv::AsyncHttpClient>> m_http_clients {};
    std::vector<std::shared_ptr<msc::PeerConnectionFactoryTuple>> m_peer_connection_factories {};

    std::string m_streamer_id {};
    std::vector<std::shared_ptr<Viewer>> m_viewers {};
};

int main(int argc, const char** argv)
{
    fmt::println("initializing...");
    msc::initialize();

    size_t num_network_thread = 4;
    size_t num_worker_thread = 4;
    size_t num_peer_connection_factory = 2;

    std::shared_ptr<ViewerManager> manager = std::make_shared<ViewerManager>(num_worker_thread, num_network_thread, num_peer_connection_factory);

    std::string streamer_id = "1268337150"; //"1174393215";
    auto input_streamer_id = ftxui::Input(&streamer_id, "");

    size_t viewer_count = 10;
    ftxui::SliderOption<size_t> slider_option;
    slider_option.value = &viewer_count;
    slider_option.min = 1;
    slider_option.max = 400;
    slider_option.increment = 1;
    slider_option.direction = ftxui::GaugeDirection::Right;
    auto input_viewer_count = ftxui::Slider(slider_option);

    auto button_apply = ftxui::Button(
        "Apply",
        [&]() {
            manager->set_streamer_id(streamer_id);
            manager->set_viewer_count(viewer_count);
        },
        ftxui::ButtonOption::Simple()
    );

    auto connection_state_gauge = [&](std::string label, int count) {
        return ftxui::hbox({
            ftxui::text(std::move(label)),
            ftxui::gauge(float(count) / float(manager->viewer_count())),
            ftxui::text(fmt::format(" {}", count))
        });
    };

    auto resolution_table = [](const std::unordered_map<std::string, int>& map) {
        std::vector<std::shared_ptr<ftxui::Node>> children;
        children.reserve(map.size());

        for (const auto& [resolution, count] : map) {
            auto line = ftxui::hbox({
                ftxui::text(resolution) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 12),
                ftxui::text(std::to_string(count))
            });

            children.push_back(std::move(line));
        }

        return ftxui::vbox(children);
    };

    auto renderer = ftxui::Renderer(
        ftxui::Container::Vertical({
            input_streamer_id,
            input_viewer_count,
            button_apply,
        }),
        [&]() {
            auto state_stats = manager->state_stats();
            auto video_stats = manager->video_stats();

            return ftxui::vbox({
                ftxui::vbox({
                    ftxui::hbox({
                        ftxui::text("Streamer id: "),
                        input_streamer_id->Render(),
                    }) | ftxui::center,
                    ftxui::vbox({
                        ftxui::text(fmt::format("Viewer count: {:4}", viewer_count)),
                        input_viewer_count->Render(),
                    }),
                    button_apply->Render() | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 20) | ftxui::center,
                }),
                ftxui::separator(),
                ftxui::hbox({
                    ftxui::vbox({
                        ftxui::text("=== Viewer State ===") | ftxui::bold,
                        connection_state_gauge("idle            ", state_stats[ViewerState::Idle]),
                        connection_state_gauge("handshake       ", state_stats[ViewerState::Handshaking]),
                        connection_state_gauge("create transport", state_stats[ViewerState::CreatingTransport]),
                        connection_state_gauge("consuming       ", state_stats[ViewerState::Consuming]),
                        connection_state_gauge("new             ", state_stats[ViewerState::New]),
                        connection_state_gauge("checking        ", state_stats[ViewerState::Checking]),
                        connection_state_gauge("connected       ", state_stats[ViewerState::Connected]),
                        connection_state_gauge("completed       ", state_stats[ViewerState::Completed]),
                        connection_state_gauge("failed          ", state_stats[ViewerState::Failed]),
                        connection_state_gauge("disconnected    ", state_stats[ViewerState::Disconnected]),
                        connection_state_gauge("closed          ", state_stats[ViewerState::Closed]),
                        ftxui::text("=== Error ===") | ftxui::bold,
                        connection_state_gauge("auth failed     ", state_stats[ViewerState::GettingAuthTokenFailed]),
                        connection_state_gauge("stream not found", state_stats[ViewerState::StreamNotFound]),
                        connection_state_gauge("consume failed  ", state_stats[ViewerState::ConsumeStreamFailed]),
                        connection_state_gauge("exception       ", state_stats[ViewerState::Exception]),
                    }) | ftxui::flex,
                    ftxui::separator(),
                    ftxui::vbox({
                        ftxui::text(fmt::format("Average FPS: {:8.4f}", video_stats.avgFps)) | ftxui::bold,
                        ftxui::separator(),
                        ftxui::text("Screen Resolutions") | ftxui::bold,
                        resolution_table(video_stats.resolution),
                    }) | ftxui::flex,
                }),
            }) | ftxui::border;
        });

    auto screen = ftxui::ScreenInteractive::Fullscreen();

    std::atomic<bool> refresh_ui_continue = true;
    std::thread refresh_ui([&] {
        while (refresh_ui_continue) {
            using namespace std::chrono_literals;

            std::this_thread::sleep_for(500ms);
            screen.Post(ftxui::Event::Custom);
        }
    });
    screen.Loop(renderer);
    refresh_ui_continue = false;
    refresh_ui.join();

    return 0;
}
