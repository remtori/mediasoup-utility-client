#pragma once

#include <hv/AsyncHttpClient.h>

#include <future>

namespace cm {

class HttpClient {
public:
    HttpClient(std::shared_ptr<hv::EventLoop> loop);
    ~HttpClient() = default;

    ::http_headers& headers() { return m_headers; }
    const ::http_headers& headers() const { return m_headers; }

    std::future<std::shared_ptr<HttpResponse>> get(const std::string& url);
    std::future<std::shared_ptr<HttpResponse>> post(const std::string& url, const nlohmann::json& body);

    std::future<std::shared_ptr<HttpResponse>> request(std::shared_ptr<HttpRequest> request);

private:
    std::shared_ptr<hv::AsyncHttpClient> m_client;
    ::http_headers m_headers { DefaultHeaders };
};

}
