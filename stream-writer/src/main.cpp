#include <chrono>

#include <hv/AsyncHttpClient.h>
#include <hv/WebSocketClient.h>
#include <msc/msc.hpp>

static void test_http_async_client(hv::AsyncHttpClient* cli)
{
    printf("test_http_async_client request thread tid=%ld\n", hv_gettid());

    HttpRequestPtr req(new HttpRequest);
    req->method = HTTP_POST;
    req->url = "https://example.com/";
    req->headers["Connection"] = "keep-alive";
    req->body = "This is an async request.";
    req->timeout = 10;
    cli->send(req, [](const HttpResponsePtr& resp) {
        printf("test_http_async_client response thread tid=%ld\n", hv_gettid());
        if (resp == nullptr) {
            printf("request failed!\n");
        } else {
            printf("%d %s\r\n", resp->status_code, resp->status_message());
            printf("%s\n", resp->body.c_str());
        }
    });
}

int main()
{
    auto event_loop_thread = std::make_shared<hv::EventLoopThread>();
    event_loop_thread->start();

    hv::AsyncHttpClient http_client(event_loop_thread->loop());
    test_http_async_client(&http_client);

    printf("Before sleep\n");
    std::this_thread::sleep_for(std::chrono::seconds(5));
    printf("After sleep\n");

    event_loop_thread->stop();
    event_loop_thread->join();
}
