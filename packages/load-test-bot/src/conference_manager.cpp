#include "./conference_manager.hpp"

#include "./timer_event_loop.hpp"
#include <random>

static uint32_t s_starting_user_id = 1;

ConferenceManager::ConferenceManager(size_t num_worker_thread, size_t num_network_thread, size_t num_peer_connection_factory)
    : m_http_client(std::make_shared<net::HttpClient>(std::make_shared<hv::AsyncHttpClient>()))
{
    std::random_device rd;
    std::srand(rd());
    m_device_id = "ltb_" + std::to_string(rd());

    m_event_loops.reserve(num_network_thread);
    for (size_t i = 0; i < num_network_thread; i++) {
        m_event_loops.push_back(std::make_shared<hv::EventLoop>());
    }

    m_executors.reserve(num_worker_thread);
    for (size_t i = 0; i < num_worker_thread; i++) {
        m_executors.push_back(std::make_shared<cm::Executor>(1));
    }

    m_peer_connection_factories.reserve(num_peer_connection_factory);
    for (size_t i = 0; i < num_peer_connection_factory; i++) {
        m_peer_connection_factories.push_back(msc::create_peer_connection_factory());
    }

    m_tick_producer_timer = timer_event_loop().setInterval(50, [this](auto) {
        this->tick_producer();
    });
}

ConferenceManager::~ConferenceManager()
{
    timer_event_loop().killTimer(m_tick_producer_timer);
    m_peers.clear();
}

void ConferenceManager::apply_config(size_t room_count, size_t user_per_room, size_t starting_room_id)
{
    if (m_room_count == room_count && m_user_per_room == user_per_room) {
        return;
    }

    m_room_count = room_count;
    m_user_per_room = user_per_room;

    std::lock_guard lk(m_mutex);
    for (auto& conference : m_peers) {
        conference->leave();
    }

    size_t required_user_count = m_room_count * m_user_per_room;
    if (m_peers.size() < required_user_count) {
        m_peers.reserve(required_user_count);
        for (auto i = m_peers.size(); i < required_user_count; i++) {
            m_peers.push_back(
                std::make_unique<ConferencePeer>(
                    m_executors[i % m_executors.size()],
                    m_event_loops[i % m_event_loops.size()],
                    m_http_client,
                    m_peer_connection_factories[i % m_peer_connection_factories.size()]));
        }
    } else if (m_peers.size() > required_user_count) {
        m_peers.resize(required_user_count);
    }

    for (size_t i = 0; i < m_room_count; i++) {
        for (size_t j = 0; j < m_user_per_room; j++) {
            auto& conference = m_peers[i * m_user_per_room + j];

            uint32_t user_id = s_starting_user_id++;
            conference->validate_data_channel(m_validate_data_channel);
            conference->joinRoom(m_device_id + "_u" + std::to_string(10000 + user_id), m_device_id + "_r" + std::to_string(starting_room_id + i));
        }
    }
}

void ConferenceManager::tick_producer()
{
    std::lock_guard lk(m_mutex);
    for (auto& conference : m_peers) {
        conference->tick_producer();
    }
}

const ConferenceManager::Stats& ConferenceManager::stats()
{
    m_stats.status.clear();
    m_stats.consume_peer.clear();
    m_stats.productive_peer = 0;
    m_stats.avg_peer_count = 0;
    m_stats.avg_frame_rate = 0;

    for (size_t i = 0; i < m_user_per_room; i++) {
        m_stats.consume_peer[i] = 0;
    }

    std::lock_guard lk(m_mutex);
    for (auto& conference : m_peers) {
        auto state = conference->state();
        m_stats.avg_frame_rate += conference->avg_frame_rate();
        m_stats.avg_peer_count += state.peer_count;
        m_stats.productive_peer += state.produce_success;
        m_stats.status[state.status]++;
        m_stats.consume_peer[state.peer_count]++;
    }

    m_stats.avg_frame_rate /= m_peers.size();
    m_stats.avg_peer_count /= m_peers.size();

    return m_stats;
}