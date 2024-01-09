#include "net/http_client.hpp"

namespace net {

HttpClient::HttpClient(std::shared_ptr<hv::AsyncHttpClient> client)
    : m_client(std::move(client))
{
}

std::future<std::shared_ptr<HttpResponse>> HttpClient::get(const std::string& url)
{
    auto req = std::make_shared<HttpRequest>();
    req->method = HTTP_GET;
    req->url = url;
    req->headers = m_headers;

    return request(req);
}

std::future<std::shared_ptr<HttpResponse>> HttpClient::post(const std::string& url, const nlohmann::json& body)
{
    auto req = std::make_shared<HttpRequest>();
    req->method = HTTP_POST;
    req->url = url;
    req->headers = m_headers;
    req->headers["Content-Type"] = "application/json";
    req->Json(body);

    return request(req);
}

std::future<std::shared_ptr<HttpResponse>> HttpClient::request(const std::shared_ptr<HttpRequest>& request)
{
    request->timeout = 10;

    auto future_resp = std::make_shared<std::promise<std::shared_ptr<HttpResponse>>>();
    m_client->send(request, [future_resp](const std::shared_ptr<HttpResponse>& resp) {
        future_resp->set_value(resp);
    });

    return future_resp->get_future();
}

void HttpClient::getAsync(const std::string& url, std::function<void(const std::shared_ptr<HttpResponse>&)> cb)
{
    auto req = std::make_shared<HttpRequest>();
    req->method = HTTP_GET;
    req->url = url;
    req->headers = m_headers;

    requestAsync(req, std::move(cb));
}

void HttpClient::postAsync(const std::string& url, const nlohmann::json& body, std::function<void(const std::shared_ptr<HttpResponse>&)> cb)
{
    auto req = std::make_shared<HttpRequest>();
    req->method = HTTP_POST;
    req->url = url;
    req->headers = m_headers;
    req->headers["Content-Type"] = "application/json";
    req->Json(body);

    requestAsync(req, std::move(cb));
}

}
