#include "viewer_manager.hpp"

#include <fmt/core.h>

ViewerManager::ViewerManager(size_t num_worker_thread, size_t num_network_thread, size_t num_peer_connection_factory)
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

void ViewerManager::set_viewer_count(size_t viewer_count)
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

std::unordered_map<ViewerState, int> ViewerManager::state_stats()
{
    std::unordered_map<ViewerState, int> stats {};
    for (const auto& viewer : m_viewers) {
        stats[viewer->state()] += 1;
    }

    return stats;
}

VideoStats ViewerManager::video_stats()
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
