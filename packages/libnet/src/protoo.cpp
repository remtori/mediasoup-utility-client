#include "net/protoo.hpp"

#include <common/logger.hpp>
#include <sstream>

namespace net {

ProtooClient::ProtooClient(std::shared_ptr<hv::EventLoop> loop)
    : WebSocketClient(std::move(loop))
{
    onopen = [this] { on_ws_open(); };
    onclose = [this] { on_ws_close(); };
    onmessage = [this](auto&& PH1) { on_ws_message(std::forward<decltype(PH1)>(PH1)); };
}

int ProtooClient::connect(const std::string& url)
{
    // reconnect: 1,2,4,8,10,10,10...
    reconn_setting_t reconn;
    reconn_setting_init(&reconn);
    reconn.min_delay = 1000;
    reconn.max_delay = 10000;
    reconn.delay_policy = 2;
    this->setReconnect(&reconn);

    return this->open(url.c_str());
}

void ProtooClient::on_ws_open()
{
    std::scoped_lock lk(m_mutex);
    for (auto& request : m_buffered_request) {
        request();
    }

    m_buffered_request.clear();
}

void ProtooClient::on_ws_message(const std::string& raw_msg)
{
    nlohmann::json msg = nlohmann::json::parse(raw_msg);
    if (!msg.is_object())
        return;

    try {
        if (msg.value("request", false)) {
            ProtooRequest request;
            msg.at("id").get_to(request.id);
            msg.at("method").get_to(request.method);
            msg.at("data").get_to(request.data);

            if (on_request)
                on_request(std::move(request));
        } else if (msg.value("response", false)) {
            ProtooResponse response;
            msg.at("ok").get_to(response.ok);
            msg.at("id").get_to(response.id);

            if (response.ok) {
                msg.at("data").get_to(response.data);
            } else {
                msg.at("errorReason").get_to(response.error_reason);
            }

            std::scoped_lock lk(m_mutex);
            if (auto it = m_awaiting_response.find(response.id); it != m_awaiting_response.end()) {
                loop()->killTimer(it->second.timeout);
                it->second.promise->set_value(std::move(response));

                m_awaiting_response.erase(it);
            }
        } else if (msg.value("notification", false)) {
            ProtooNotify notification;
            msg.at("method").get_to(notification.method);
            msg.at("data").get_to(notification.data);

            if (on_notify)
                on_notify(std::move(notification));
        }
    } catch (const std::exception& ex) {
        cm::log("[ProtooClient][ERROR] {}", ex.what());
    }
}

void ProtooClient::on_ws_close() const
{
    if (on_close)
        on_close();
}

void ProtooClient::notify(std::string method, nlohmann::json data)
{
    nlohmann::json body = {
        { "notification", true },
        { "method", std::move(method) },
        { "data", std::move(data) },
    };

    auto raw_body = body.dump();
    if (this->isConnected()) {
        send(raw_body);
    } else {
        m_buffered_request.emplace_back([this, raw_body = std::move(raw_body)]() {
            this->send(raw_body);
        });
    }
}

std::future<ProtooResponse> ProtooClient::request(std::string method, nlohmann::json data)
{
    auto ret = std::make_shared<std::promise<ProtooResponse>>();

    uint64_t id = m_request_id_gen.fetch_add(1);
    nlohmann::json body = {
        { "request", true },
        { "id", id },
        { "method", std::move(method) },
        { "data", std::move(data) },
    };

    auto timeout = loop()->setTimeout(10000, [=](auto) {
        ret->set_exception(std::make_exception_ptr(std::runtime_error("request timeout, method=" + method)));

        std::scoped_lock lk(m_mutex);
        m_awaiting_response.erase(id);
    });

    {
        std::scoped_lock lk(m_mutex);
        m_awaiting_response.insert({ id, PendingRequest { ret, timeout } });
    }

    auto raw_body = body.dump();
    if (this->isConnected()) {
        send(raw_body);
    } else {
        m_buffered_request.emplace_back([this, raw_body = std::move(raw_body)]() {
            this->send(raw_body);
        });
    }

    return ret->get_future();
}

void ProtooClient::response(ProtooResponse response)
{
    nlohmann::json body;
    if (response.ok) {
        body = {
            { "response", true },
            { "ok", true },
            { "id", response.id },
            { "method", "ws-response" },
            { "data", std::move(response.data) },
        };
    } else {
        body = {
            { "response", true },
            { "ok", false },
            { "id", response.id },
            { "method", "ws-response" },
            { "errorReason", std::move(response.error_reason) },
        };
    }

    auto raw_body = body.dump();
    send(raw_body);
}
}
