#pragma once

#include <hv/AsyncHttpClient.h>

#include <future>

namespace cm {

class HttpClient {
public:
    explicit HttpClient(std::shared_ptr<hv::AsyncHttpClient>);
    ~HttpClient() = default;

    [[nodiscard]] ::http_headers& headers() { return m_headers; }
    [[nodiscard]] const ::http_headers& headers() const { return m_headers; }

    std::future<std::shared_ptr<HttpResponse>> get(const std::string& url);
    std::future<std::shared_ptr<HttpResponse>> post(const std::string& url, const nlohmann::json& body);

    std::future<std::shared_ptr<HttpResponse>> request(const std::shared_ptr<HttpRequest>& request);

    void getAsync(const std::string& url, std::function<void(const std::shared_ptr<HttpResponse>&)>);
    void postAsync(const std::string& url, const nlohmann::json& body, std::function<void(const std::shared_ptr<HttpResponse>&)>);

    inline void requestAsync(const std::shared_ptr<HttpRequest>& request, std::function<void(const std::shared_ptr<HttpResponse>&)> cb) {
        m_client->send(request, std::move(cb));
    }

private:
    std::shared_ptr<hv::AsyncHttpClient> m_client;
    ::http_headers m_headers { DefaultHeaders };
};

}
