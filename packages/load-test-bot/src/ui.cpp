#include "./ui.hpp"

#include <chrono>
#include <fmt/core.h>

#include <ftxui/component/captured_mouse.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

void setup_livestream_bot_ui(std::shared_ptr<ViewerManager> manager, size_t default_viewer_count)
{
    std::string streamer_id = manager->streamer_id();
    auto input_streamer_id = ftxui::Input(&streamer_id, "Streamer ID");

    size_t viewer_count = default_viewer_count;
    ftxui::SliderOption<size_t> slider_option;
    slider_option.value = &viewer_count;
    slider_option.min = size_t(0);
    slider_option.max = 200;
    slider_option.increment = 1;
    slider_option.direction = ftxui::GaugeDirection::Right;
    auto input_viewer_count = ftxui::Slider(slider_option);

    auto button_apply = ftxui::Button(
        "Apply",
        [&]() {
            manager->set_streamer_id(streamer_id);
            manager->set_viewer_count(viewer_count);
        },
        ftxui::ButtonOption::Simple());

    auto connection_state_gauge = [&](std::string label, int count) {
        return ftxui::hbox({ ftxui::text(std::move(label)),
            ftxui::gauge(float(count) / float(manager->viewer_count())),
            ftxui::text(fmt::format(" {}", count)) });
    };

    auto resolution_table = [](const std::unordered_map<std::string, int>& map) {
        std::vector<std::shared_ptr<ftxui::Node>> children;
        children.reserve(map.size());

        for (const auto& [resolution, count] : map) {
            auto line = ftxui::hbox({ ftxui::text(resolution) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 12),
                ftxui::text(std::to_string(count)) });

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
                   })
                | ftxui::border;
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
}

void setup_conference_bot_ui(std::shared_ptr<ConferenceManager> manager, size_t default_room_count, size_t default_user_count, size_t default_base_room_id)
{
    size_t room_count = default_room_count;
    size_t user_count = default_user_count;
    std::string base_room_id = std::to_string(default_base_room_id);

    ftxui::SliderOption<size_t> rc_slider_option;
    rc_slider_option.value = &room_count;
    rc_slider_option.min = size_t(0);
    rc_slider_option.max = 400;
    rc_slider_option.increment = 1;
    rc_slider_option.direction = ftxui::GaugeDirection::Right;
    auto input_room_count = ftxui::Slider(rc_slider_option);

    ftxui::SliderOption<size_t> uc_slider_option;
    uc_slider_option.value = &user_count;
    uc_slider_option.min = size_t(0);
    uc_slider_option.max = 20;
    uc_slider_option.increment = 1;
    uc_slider_option.direction = ftxui::GaugeDirection::Right;
    auto input_user_count = ftxui::Slider(uc_slider_option);

    auto input_base_room_id = ftxui::Input(&base_room_id, "Base room id");
    input_base_room_id |= ftxui::CatchEvent([](ftxui::Event event) {
        return event.is_character() && !std::isdigit(event.character()[0]);
    });

    auto button_apply = ftxui::Button(
        "Apply",
        [&]() {
            manager->apply_config(room_count, user_count, std::stoul(base_room_id));
        },
        ftxui::ButtonOption::Simple());

    auto gauge = [&](std::string label, int count) {
        return ftxui::hbox({ ftxui::text(std::move(label)),
            ftxui::gauge(float(count) / float(manager->total_user_count())),
            ftxui::text(fmt::format(" {}", count)) });
    };

    auto consumer_count_gauge = [&](const std::unordered_map<size_t, uint32_t>& map) {
        std::vector<std::shared_ptr<ftxui::Node>> children;
        children.reserve(map.size());

        for (const auto& [num, count] : map) {
            auto line = ftxui::hbox({ ftxui::text(fmt::format("consume {:02} peer ", num)),
                ftxui::gauge(float(count) / float(manager->total_user_count())),
                ftxui::text(fmt::format(" {}", count)) });

            children.push_back(std::move(line));
        }

        return ftxui::vbox(children);
    };

    auto renderer
        = ftxui::Renderer(
            ftxui::Container::Vertical({
                input_base_room_id,
                input_room_count,
                input_user_count,
                button_apply,
            }),
            [&]() {
                auto stats = manager->stats();

                return ftxui::vbox({
                           ftxui::vbox({
                               ftxui::hbox({
                                   ftxui::text("Base room id: "),
                                   input_base_room_id->Render(),
                               }) | ftxui::center,
                               ftxui::vbox({
                                   ftxui::text(fmt::format("Room count: {:4}", room_count)),
                                   input_room_count->Render(),
                               }),
                               ftxui::vbox({
                                   ftxui::text(fmt::format("User per room: {:4}", user_count)),
                                   input_user_count->Render(),
                               }),
                               button_apply->Render() | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 20) | ftxui::center,
                           }),
                           ftxui::separator(),
                           ftxui::hbox({
                               ftxui::vbox({
                                   ftxui::text("=== Conference State ===") | ftxui::bold,
                                   gauge("idle            ", stats.status[ConferenceStatus::Idle]),
                                   gauge("new             ", stats.status[ConferenceStatus::New]),
                                   gauge("checking        ", stats.status[ConferenceStatus::Checking]),
                                   gauge("connecting      ", stats.status[ConferenceStatus::Connecting]),
                                   gauge("connected       ", stats.status[ConferenceStatus::Connected]),
                                   gauge("completed       ", stats.status[ConferenceStatus::Completed]),
                                   gauge("failed          ", stats.status[ConferenceStatus::Failed]),
                                   gauge("disconnected    ", stats.status[ConferenceStatus::Disconnected]),
                                   gauge("closed          ", stats.status[ConferenceStatus::Closed]),
                                   ftxui::text("=== Error ===") | ftxui::bold,
                                   gauge("exception       ", stats.status[ConferenceStatus::Exception]),
                               }) | ftxui::flex,
                               ftxui::separator(),
                               ftxui::vbox({
                                   ftxui::text(fmt::format("Average send FPS: {:8.4f}", stats.avg_send_frame_rate)) | ftxui::bold,
                                   ftxui::text(fmt::format("Average recv FPS: {:8.4f}", stats.avg_recv_frame_rate)) | ftxui::bold,
                                   ftxui::text("=== Peer Statistics ===") | ftxui::bold,
                                   gauge("productive peer ", stats.productive_peer),
                                   consumer_count_gauge(stats.consume_peer),
                               }) | ftxui::flex,
                           }),
                       })
                    | ftxui::border;
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
}