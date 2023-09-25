#pragma once

#include <hv/WebSocketClient.h>

#include <functional>
#include <future>
#include <memory>
#include <string>
#include <vector>

#include "./json.hpp"

namespace cm {

struct ProtooRequest {
    int64_t id;

    std::string method;
    nlohmann::json data;
};

struct ProtooResponse {
    bool ok;
    int64_t id;

    std::string method;
    nlohmann::json data;

    int64_t error_code;
    nlohmann::json error_reason;
};

struct ProtooNotify {
    std::string method;
    nlohmann::json data;
};

class ProtooClient : private hv::WebSocketClient {
public:
    explicit ProtooClient(std::shared_ptr<hv::EventLoop> loop);
    ~ProtooClient() = default;

    int connect(const std::string& url);

    void notify(std::string method, nlohmann::json data);
    std::future<ProtooResponse> request(std::string method, nlohmann::json data);
    void response(ProtooResponse response);

    std::function<void(ProtooNotify)> on_notify = {};
    std::function<void(ProtooRequest)> on_request = {};
    std::function<void()> on_close = {};

private:
    void on_ws_open();
    void on_ws_message(const std::string& msg);
    void on_ws_close() const;

private:
    std::mutex m_mutex {};
    std::atomic_uint64_t m_request_id_gen { 1 };
    std::unordered_map<uint64_t, std::shared_ptr<std::promise<ProtooResponse>>> m_awaiting_response {};

    std::vector<std::function<void()>> m_buffered_request {};
};

}
