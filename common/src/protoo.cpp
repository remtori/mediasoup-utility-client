#include "common/protoo.hpp"

#include <sstream>

namespace cm {

ProtooClient::ProtooClient(std::shared_ptr<hv::EventLoop> loop)
    : WebSocketClient(loop)
{
    onopen = std::bind(&ProtooClient::on_ws_open, this);
    onclose = std::bind(&ProtooClient::on_ws_close, this);
    onmessage = std::bind(&ProtooClient::on_ws_message, this, std::placeholders::_1);
}

int ProtooClient::connect(const std::string& url, std::vector<std::string> protocols)
{

    // reconnect: 1,2,4,8,10,10,10...
    reconn_setting_t reconn;
    reconn_setting_init(&reconn);
    reconn.min_delay = 1000;
    reconn.max_delay = 10000;
    reconn.delay_policy = 2;
    this->setReconnect(&reconn);

    std::stringstream ss;
    ss << "protoo";
    for (auto& protocol : protocols)
        ss << ", " << protocol;

    http_headers headers;
    headers["Sec-WebSocket-Protocol"] = ss.str();
    return this->open(url.c_str(), headers);
}

void ProtooClient::on_ws_open()
{
    const std::scoped_lock lk(m_mutex);
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
            msg.at("method").get_to(response.method);
            msg.at("data").get_to(response.data);
        } else {
            msg.at("errorCode").get_to(response.error_code);
            msg.at("errorReason").get_to(response.error_reason);
        }

        const std::scoped_lock lk(m_mutex);
        if (auto it = m_awaiting_response.find(response.id); it != m_awaiting_response.end()) {
            it->second->set_value(std::move(response));
        }
    } else if (msg.value("notification", false)) {
        ProtooNotify notification;
        msg.at("method").get_to(notification.method);
        msg.at("data").get_to(notification.data);

        if (on_notify)
            on_notify(std::move(notification));
    }
}

void ProtooClient::on_ws_close()
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
    send(raw_body);
}

std::future<ProtooResponse> ProtooClient::request(std::string method, nlohmann::json data)
{
    auto ret = std::make_shared<std::promise<ProtooResponse>>();

    auto id = m_request_id_gen.fetch_add(1);
    nlohmann::json body = {
        { "request", true },
        { "id", id },
        { "method", std::move(method) },
        { "data", std::move(data) },
    };

    {
        const std::scoped_lock lk(m_mutex);
        m_awaiting_response.insert({ id, ret });
    }

    auto raw_body = body.dump();
    send(raw_body);

    loop()->setTimeout(10000, [ret](auto) {
        ret->set_exception(std::make_exception_ptr(std::runtime_error("request timeout")));
    });

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
            { "method", std::move(response.method) },
            { "data", std::move(response.data) },
        };
    } else {
        body = {
            { "response", true },
            { "ok", false },
            { "id", response.id },
            { "method", std::move(response.method) },
            { "errorCode", response.error_code },
            { "errorReason", std::move(response.error_reason) },
        };
    }

    auto raw_body = body.dump();
    send(raw_body);
}

}
