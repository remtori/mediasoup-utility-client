#pragma once

#include <common/executor.hpp>
#include <common/json.hpp>

enum class Result {
    Ok,
    Error,
};

class Channel {
public:
    enum MessageType {
        TypeNotification = 0,
        TypeRequest = 1,
        TypeResponseSuccess = 2,
        TypeResponseFailure = 3,
    };

    struct Response {
        int64_t id;
        nlohmann::json data;
    };

    struct Request {
        int64_t id;
        std::string method;
        nlohmann::json data;
    };

private:
    std::thread m_thread {};
    std::mutex m_mutex {};
    std::unordered_map<int64_t, std::promise<nlohmann::json>> m_promises {};
    int64_t m_id_gen { 1 };
    bool m_running { true };

public:
    Channel()
    {
    }

    ~Channel();

    void start();

    nlohmann::json request(std::string, nlohmann::json);
    void notify(std::string, nlohmann::json);
    void response(Request, Result, nlohmann::json);

    std::function<void(std::string, nlohmann::json)> on_notify = {};
    std::function<void(Request)> on_request = {};

private:
    void worker_thread();
};
