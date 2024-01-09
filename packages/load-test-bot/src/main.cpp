#include <argparse/argparse.hpp>
#include <fmt/format.h>

#include <common/logger.hpp>
#include <msc/msc.hpp>

#include "./conference_manager.hpp"
#include "./timer_event_loop.hpp"
#include "./ui.hpp"
#include "./viewer_manager.hpp"

struct CommonConfig {
    bool use_gui;
    size_t num_network_thread;
    size_t num_worker_thread;
    size_t num_peer_connection_factory;
};

void run_livestream_view_bot(const argparse::ArgumentParser& program, CommonConfig config);
void run_conference_bot(const argparse::ArgumentParser& program, CommonConfig config);

int main(int argc, const char** argv)
{
    argparse::ArgumentParser program("load_test_bot");

    program.add_argument("--nogui")
        .help("disable interactive GUI")
        .default_value(false)
        .implicit_value(true);

    program.add_argument("-w", "--worker-thread")
        .help("Number of worker thread for blocking mediasoup call")
        .scan<'u', size_t>()
        .metavar("UINT")
        .default_value(size_t(4));
    program.add_argument("-n", "--network-thread")
        .help("Number of network thread for API call")
        .scan<'u', size_t>()
        .metavar("UINT")
        .default_value(size_t(4));
    program.add_argument("-p", "--peer-factory")
        .help("Number of peer connection factory, each with its own network, worker and signaling thread.")
        .scan<'u', size_t>()
        .metavar("UINT")
        .default_value(size_t(1));

    argparse::ArgumentParser livestream_view_bot("livestream");
    livestream_view_bot.add_argument("-i", "--streamer-id")
        .default_value(std::string(true ? "1174393215" : "1268337150"));
    livestream_view_bot.add_argument("-v", "--viewer")
        .help("Number of viewer")
        .scan<'u', size_t>()
        .metavar("UINT")
        .default_value(size_t(10));

    argparse::ArgumentParser conference_bot("conference");
    conference_bot.add_argument("-r", "--room-count")
        .help("Number of room")
        .scan<'u', size_t>()
        .metavar("UINT")
        .default_value(size_t(10));
    conference_bot.add_argument("-u", "--user-per-room")
        .help("Number of user per room")
        .scan<'u', size_t>()
        .metavar("UINT")
        .default_value(size_t(4));
    conference_bot.add_argument("--rid")
        .help("Starting room id")
        .scan<'u', size_t>()
        .metavar("UINT")
        .default_value(size_t(0));

    program.add_subparser(livestream_view_bot);
    program.add_subparser(conference_bot);

    try {
        program.parse_args(argc, argv);
    } catch (const std::runtime_error& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    }

    bool use_gui = program["--nogui"] == false;
    cm::init_logger(use_gui ? "load_test.log" : nullptr);

    cm::log("Initializing...");
    msc::initialize();
    cm::log("Initialized");

    CommonConfig config {
        .use_gui = use_gui,
        .num_network_thread = program.get<size_t>("-n"),
        .num_worker_thread = program.get<size_t>("-w"),
        .num_peer_connection_factory = program.get<size_t>("-p"),
    };

    try {
        if (program.is_subcommand_used("livestream")) {
            run_livestream_view_bot(livestream_view_bot, config);
        } else if (program.is_subcommand_used("conference")) {
            fmt::println("Running conference bot with \n{} network_thread(s)\n{} worker_thread(s)\n{} peer_connection_factory(ies)\n", config.num_network_thread, config.num_worker_thread, config.num_peer_connection_factory);
            run_conference_bot(conference_bot, config);
        } else {
            std::cout << program;
            return 0;
        }
    } catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::exit(1);
    }

    return 0;
}

void run_livestream_view_bot(const argparse::ArgumentParser& program, CommonConfig config)
{
    std::string streamer_id = program.get<std::string>("--streamer-id");
    size_t viewer_count = program.get<size_t>("--viewer");

    std::shared_ptr<ViewerManager> manager = std::make_shared<ViewerManager>(config.num_worker_thread, config.num_network_thread, config.num_peer_connection_factory);
    manager->set_streamer_id(streamer_id);

    if (config.use_gui) {
        setup_livestream_bot_ui(manager, viewer_count);
        return;
    }

    std::thread refresh_ui([&] {
        while (true) {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(500ms);

            auto video_stats = manager->video_stats();
            auto state_stats = manager->state_stats();

            size_t init = state_stats[ViewerState::Idle] + state_stats[ViewerState::Handshaking] + state_stats[ViewerState::CreatingTransport] + state_stats[ViewerState::Consuming] + state_stats[ViewerState::New] + state_stats[ViewerState::Checking] + state_stats[ViewerState::Connected];
            size_t success = state_stats[ViewerState::Completed];
            size_t failed = state_stats[ViewerState::Failed] + state_stats[ViewerState::Disconnected] + state_stats[ViewerState::Closed];
            size_t error = state_stats[ViewerState::GettingAuthTokenFailed] + state_stats[ViewerState::StreamNotFound] + state_stats[ViewerState::ConsumeStreamFailed] + state_stats[ViewerState::Exception];

            auto out = fmt::format("\r[init={:2} ok={:2} fail={:2} err={:2} | avgFps={:8.4f}]", init, success, failed, error, video_stats.avgFps);
            std::cout << out;
            std::cout.flush();

            if (failed + error == manager->viewer_count())
                return;
        }
    });

    manager->set_viewer_count(viewer_count);
    refresh_ui.join();
}

void run_conference_bot(const argparse::ArgumentParser& program, CommonConfig config)
{
    size_t room_count = program.get<size_t>("--room-count");
    size_t user_count = program.get<size_t>("--user-per-room");
    size_t room_id = program.get<size_t>("--rid");

    std::shared_ptr<ConferenceManager> manager = std::make_shared<ConferenceManager>(config.num_worker_thread, config.num_network_thread, config.num_peer_connection_factory);

    if (config.use_gui) {
        setup_conference_bot_ui(manager, room_count, user_count, room_id);
        return;
    }

    manager->apply_config(room_count, user_count, room_id);
    timer_event_loop_thread().join();

    cm::log("Exit somewhat gracefully\n");
}
