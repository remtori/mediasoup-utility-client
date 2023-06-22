#pragma once

#include "./lib.h"

#include <mediasoupclient.hpp>

namespace impl {

class Device
    : public mediasoupclient::SendTransport::Listener
    , public mediasoupclient::RecvTransport::Listener
    , public mediasoupclient::Producer::Listener
    , public mediasoupclient::Consumer::Listener {
public:
    Device(void* ctx,
        ::OnConnect on_connect_handler,
        ::OnConnectionStateChange on_connection_state_change_handler,
        ::OnProduce on_produce_handler)
        : m_user_ctx(ctx)
        , m_on_connect_handler(on_connect_handler)
        , m_on_connection_state_change_handler(on_connection_state_change_handler)
        , m_on_produce_handler(on_produce_handler)
    {
    }

    virtual ~Device() { }

    std::future<void> OnConnect(mediasoupclient::Transport*, const nlohmann::json&) override;
    void OnConnectionStateChange(mediasoupclient::Transport*, const std::string&) override;
    std::future<std::string> OnProduce(mediasoupclient::SendTransport*, const std::string&, nlohmann::json, const nlohmann::json&) override;
    std::future<std::string> OnProduceData(mediasoupclient::SendTransport*, const nlohmann::json&, const std::string&, const std::string&, const nlohmann::json&) override;
    void OnTransportClose(mediasoupclient::Producer* producer) override;
    void OnTransportClose(mediasoupclient::Consumer* consumer) override;

    mediasoupclient::Device& device() { return m_device; }

    void fulfill_producer_id(int64_t promise_id, const std::string& producer_id)
    {
        auto it = m_map_promise_producer_id.find(promise_id);
        it->second.set_value(producer_id);
        m_map_promise_producer_id.erase(it);
    }

private:
    mediasoupclient::Device m_device {};

    void* m_user_ctx { nullptr };
    ::OnConnect m_on_connect_handler { nullptr };
    ::OnConnectionStateChange m_on_connection_state_change_handler { nullptr };
    ::OnProduce m_on_produce_handler { nullptr };

    std::unordered_map<int64_t, std::promise<std::string>> m_map_promise_producer_id {};
    std::atomic_int64_t m_map_id_gen {};
};

}