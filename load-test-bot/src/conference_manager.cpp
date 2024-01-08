#include "./conference_manager.hpp"

#include "./timer_event_loop.hpp"

ConferenceManager::ConferenceManager(size_t num_worker_thread, size_t num_network_thread, size_t num_peer_connection_factory)
    : m_http_client(std::make_shared<cm::HttpClient>(std::make_shared<hv::AsyncHttpClient>()))
{
    m_device_id = "ltb_" + std::to_string(std::rand());

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

    m_tick_producer_timer = timer_event_loop().setInterval(10, [this](auto) {
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
            conference->joinRoom(m_device_id + "_user_" + std::to_string(j), "werewolf_" + std::to_string(starting_room_id + i + 10000));
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
