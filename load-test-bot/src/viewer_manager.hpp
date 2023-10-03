#pragma once

#include <common/http_client.hpp>

#include "viewer.hpp"

struct VideoStats {
    float avgFps;
    std::unordered_map<std::string, int> resolution;
};

class ViewerManager {
public:
    explicit ViewerManager(size_t num_worker_thread, size_t num_network_thread, size_t num_peer_connection_factory);

    void set_streamer_id(std::string streamer_id)
    {
        m_streamer_id = std::move(streamer_id);
    }

    const std::string& streamer_id()
    {
        return m_streamer_id;
    }

    void set_viewer_count(size_t viewer_count);

    size_t viewer_count() const
    {
        return m_viewers.size();
    }

    std::unordered_map<ViewerState, int> state_stats();
    VideoStats video_stats();
private:
    std::unique_ptr<cm::Executor> m_executor;
    std::vector<std::shared_ptr<hv::AsyncHttpClient>> m_http_clients {};
    std::vector<std::shared_ptr<msc::PeerConnectionFactoryTuple>> m_peer_connection_factories {};

    std::string m_streamer_id {};
    std::vector<std::shared_ptr<Viewer>> m_viewers {};
};