#include <argparse/argparse.hpp>
#include <fmt/format.h>

#include <common/logger.hpp>
#include <msc/msc.hpp>

#include "./ui.hpp"
#include "./viewer_manager.hpp"

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
        .default_value(size_t(4));
    program.add_argument("-n", "--network-thread")
        .help("Number of network thread for API call")
        .scan<'u', size_t>()
        .default_value(size_t(4));
    program.add_argument("-p", "--peer-factory")
        .help("Number of peer connection factory, each with its own network, worker and signaling thread.")
        .scan<'u', size_t>()
        .default_value(size_t(1));

    program.add_argument("-i", "--streamer-id")
        .default_value(std::string(true ? "1174393215" : "1268337150"));

    program.add_argument("-v", "--viewer")
        .help("Number of viewer")
        .scan<'u', size_t>()
        .default_value(size_t(10));

    try {
        program.parse_args(argc, argv);
    } catch (const std::runtime_error& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    }

    bool no_gui = program["--nogui"] == true;
    cm::init_logger(no_gui ? nullptr : "load_test.log");

    cm::log("Initializing...");
    msc::initialize();
    cm::log("Initialized");

    size_t num_network_thread = program.get<size_t>("-n");
    size_t num_worker_thread = program.get<size_t>("-w");
    size_t num_peer_connection_factory = program.get<size_t>("-p");

    std::string streamer_id = program.get<std::string>("--streamer-id");
    size_t viewer_count = program.get<size_t>("--viewer");

    std::shared_ptr<ViewerManager> manager = std::make_shared<ViewerManager>(num_worker_thread, num_network_thread, num_peer_connection_factory);
    manager->set_streamer_id(streamer_id);

    if (!no_gui) {
        setup_ui(manager, viewer_count);
        return 0;
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

    return 0;
}
