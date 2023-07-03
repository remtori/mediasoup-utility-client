#include <iostream>

#include <msc/msc.hpp>

#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>

using ws_client = websocketpp::client<websocketpp::config::asio_tls_client>;
using ws_context_ptr = websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context>;

int main()
{
    ws_client c;
    c.set_access_channels(websocketpp::log::alevel::all);
    c.clear_access_channels(websocketpp::log::alevel::frame_payload);
    c.set_error_channels(websocketpp::log::elevel::all);
    c.init_asio();

    auto executor = std::make_shared<msc::Executor>();
    auto device = msc::Device::create(executor, nullptr);
    std::cout << "rtp_capabilities: " << device->rtp_capabilities().dump() << std::endl;

    return 0;
}
