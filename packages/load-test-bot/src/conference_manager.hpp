#pragma once

#include "./conference.hpp"

#include <mutex>

class ConferenceManager {
public:
    struct Stats {
        std::unordered_map<ConferenceStatus, uint32_t> status {};
        std::unordered_map<size_t, uint32_t> consume_peer {};
        size_t productive_peer { 0 };
        float avg_peer_count { 0 };
        float avg_frame_rate { 0 };
    };

private:
    std::string m_device_id;
    std::shared_ptr<net::HttpClient> m_http_client;
    std::vector<hv::EventLoopPtr> m_event_loops {};
    std::vector<std::shared_ptr<cm::Executor>> m_executors {};
    std::vector<std::shared_ptr<msc::PeerConnectionFactoryTuple>> m_peer_connection_factories {};

    std::mutex m_mutex {};
    std::vector<std::unique_ptr<ConferencePeer>> m_peers {};
    size_t m_room_count { 1 };
    size_t m_user_per_room { 4 };

    hv::TimerID m_tick_producer_timer {};
    Stats m_stats {};

public:
    ConferenceManager(size_t num_worker_thread, size_t num_network_thread, size_t num_peer_connection_factory);
    ~ConferenceManager();

    void apply_config(size_t room_count, size_t user_per_room, size_t starting_room_id = 0);

    const Stats& stats();

private:
    void tick_producer();
};