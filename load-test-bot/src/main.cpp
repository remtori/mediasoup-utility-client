#include <chrono>
#include <iostream>

#include <ftxui/component/captured_mouse.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
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
    explicit ViewerManager(size_t num_worker_thread, size_t num_network_thread)
        : m_executor(std::make_unique<cm::Executor>(num_worker_thread))
    {
        m_http_clients.reserve(num_network_thread);
        for (auto i = 0; i < num_network_thread; i++) {
            m_http_clients.push_back(std::make_shared<hv::AsyncHttpClient>());
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
                auto viewer = std::make_shared<Viewer>(http_client);

                m_viewers.push_back(viewer);
                m_executor->push_task([this, viewer]() {
                    viewer->watch(m_streamer_id);
                });
            }
        }
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

    std::string m_streamer_id {};
    std::vector<std::shared_ptr<Viewer>> m_viewers {};
};

int main(int argc, const char** argv)
{
    fmt::println("initializing...");
    msc::initialize();

    size_t num_network_thread = 4;
    size_t num_worker_thread = std::thread::hardware_concurrency();

    std::shared_ptr<ViewerManager> manager = std::make_shared<ViewerManager>(num_worker_thread, num_network_thread);

    std::string streamer_id = "1174393215";
    auto input_streamer_id = ftxui::Input(&streamer_id, "");

    size_t viewer_count = 40;
    ftxui::SliderOption<size_t> slider_option;
    slider_option.value = &viewer_count;
    slider_option.min = 1;
    slider_option.max = 1000;
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
            ftxui::gauge(float(count) / float(viewer_count)),
            ftxui::text(fmt::format(" {}", count))
        });
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
                        ftxui::text(fmt::format("Average FPS: {:6.4f}", video_stats.avgFps)) | ftxui::bold,
                        ftxui::separator(),
                        ftxui::text("Screen Resolutions") | ftxui::bold,
                    }) | ftxui::flex,
                }),
            }) | ftxui::border;
        });

    auto screen = ftxui::ScreenInteractive::Fullscreen();
    screen.Loop(renderer);

    return 0;
}
