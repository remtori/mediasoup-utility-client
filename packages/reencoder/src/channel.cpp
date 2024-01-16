#include "./channel.hpp"

#include <common/logger.hpp>
#include <iostream>

Channel::~Channel()
{
}

void Channel::start()
{
    m_thread = std::move(std::thread(std::bind(&Channel::worker_thread, this)));
    m_thread.detach();
}

nlohmann::json Channel::request(std::string method, nlohmann::json body)
{
    int64_t id = m_id_gen++;
    std::promise<nlohmann::json> promise;
    auto future = promise.get_future();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_promises.emplace(id, std::move(promise));
    }

    std::cout << nlohmann::json({
                                    { "type", TypeRequest },
                                    { "id", id },
                                    { "method", method },
                                    { "data", body },
                                })
                     .dump()
              << std::endl;

    return future.get();
}

void Channel::notify(std::string method, nlohmann::json data)
{
    std::cout << nlohmann::json({
                                    { "type", TypeNotification },
                                    { "method", method },
                                    { "data", data },
                                })
                     .dump()
              << std::endl;
}

void Channel::response(Request req, Result res, nlohmann::json data)
{
    std::cout << nlohmann::json({
                                    { "id", req.id },
                                    { "type", res == Result::Ok ? TypeResponseSuccess : TypeResponseFailure },
                                    { "data", data },
                                })
                     .dump()
              << std::endl;
}

void Channel::worker_thread()
{
    std::string raw_buffer;
    raw_buffer.reserve(1024 * 16);

    while (m_running) {
        if (!std::getline(std::cin, raw_buffer, '\n')) {
            break;
        }

        if (!m_running) {
            break;
        }

        try {
            auto message = nlohmann::json::parse(raw_buffer);
            int type = message.at("type").get<int>();
            if (type == TypeNotification && on_notify) {
                on_notify(message.at("method").get<std::string>(), message.at("data"));
            } else if (type == TypeRequest && on_request) {
                on_request(Request {
                    .id = message.at("id").get<int64_t>(),
                    .method = message.at("method").get<std::string>(),
                    .data = std::move(message.at("data")),
                });
            } else {
                auto id = message.at("id").get<int64_t>();
                auto it = m_promises.find(id);
                if (it != m_promises.end()) {
                    if (type == TypeResponseSuccess) {
                        it->second.set_value(message.at("data"));
                    } else {
                        it->second.set_exception(std::make_exception_ptr(std::runtime_error(message.at("data").dump(2))));
                    }

                    m_promises.erase(it);
                }
            }
        } catch (const std::exception& ex) {
            cm::log("SocketWorkerError: {}", ex.what());
        }
    }
}