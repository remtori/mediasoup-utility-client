#pragma once

#include <hv/WebSocketClient.h>

#include <functional>
#include <future>
#include <memory>
#include <string>
#include <vector>

#include <common/json.hpp>

namespace net {

struct ProtooResponse {
    bool ok;
    int64_t id;

    nlohmann::json data;
    nlohmann::json error_reason;
};

struct ProtooRequest {
    int64_t id;

    std::string method;
    nlohmann::json data;

    ProtooResponse ok(nlohmann::json data)
    {
        return ProtooResponse {
            .ok = true,
            .id = id,
            .data = std::move(data),
            .error_reason = nullptr,
        };
    }

    ProtooResponse err(nlohmann::json error_reason)
    {
        return ProtooResponse {
            .ok = false,
            .id = id,
            .data = nullptr,
            .error_reason = std::move(error_reason),
        };
    }
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
    using hv::WebSocketClient::close;

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

    struct PendingRequest {
        std::shared_ptr<std::promise<ProtooResponse>> promise;
        hv::TimerID timeout;
    };

private:
    std::mutex m_mutex {};
    std::atomic_uint64_t m_request_id_gen { 1 };
    std::unordered_map<uint64_t, PendingRequest> m_awaiting_response {};

    std::vector<std::function<void()>> m_buffered_request {};
};

}
