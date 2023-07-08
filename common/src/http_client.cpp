#include "common/http_client.hpp"

namespace cm {

HttpClient::HttpClient(std::shared_ptr<hv::EventLoop> loop)
    : m_client(std::make_shared<hv::AsyncHttpClient>(loop))
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
    req->Json(body);

    req->headers["Content-Type"] = "application/json";

    return request(req);
}

std::future<std::shared_ptr<HttpResponse>> HttpClient::request(std::shared_ptr<HttpRequest> request)
{
    request->timeout = 10;

    auto future_resp = std::make_shared<std::promise<std::shared_ptr<HttpResponse>>>();
    m_client->send(request, [future_resp](const std::shared_ptr<HttpResponse> resp) {
        future_resp->set_value(resp);
    });

    return future_resp->get_future();
}

}
