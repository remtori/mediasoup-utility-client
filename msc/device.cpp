#include "./device.hpp"

namespace impl {

std::future<void> Device::OnConnect(mediasoupclient::Transport* transport, const nlohmann::json& dtlsParameters)
{
    if (m_on_connect_handler) {
        auto str = dtlsParameters.dump();
        m_on_connect_handler(
            reinterpret_cast<::MscDevice*>(this),
            reinterpret_cast<::MscTransport*>(transport),
            m_user_ctx,
            str.c_str());
    }

    std::promise<void> ret;
    ret.set_value();
    return ret.get_future();
}

void Device::OnConnectionStateChange(mediasoupclient::Transport* transport, const std::string& connectionState)
{
    if (m_on_connection_state_change_handler) {
        m_on_connection_state_change_handler(
            reinterpret_cast<::MscDevice*>(this),
            reinterpret_cast<::MscTransport*>(transport),
            m_user_ctx,
            connectionState.c_str());
    }
}

std::future<std::string> Device::OnProduce(mediasoupclient::SendTransport* transport, const std::string& kind, nlohmann::json rtpParameters, const nlohmann::json&)
{
    std::promise<std::string> promise;
    if (m_on_produce_handler) {
        auto future = promise.get_future();
        auto promise_id = m_map_id_gen.fetch_add(1);
        m_map_promise_producer_id.insert({ promise_id, std::move(promise) });

        auto str = rtpParameters.dump();
        m_on_produce_handler(
            reinterpret_cast<::MscDevice*>(this),
            reinterpret_cast<::MscTransport*>(transport),
            m_user_ctx,
            promise_id,
            kind == "audio" ? ::MscMediaKind::Audio : ::MscMediaKind::Video,
            str.c_str());

        return future;
    }

    promise.set_exception(std::make_exception_ptr("OnProduce(): null on_produce_handler handler"));
    return promise.get_future();
}

std::future<std::string> Device::OnProduceData(mediasoupclient::SendTransport*, const nlohmann::json&, const std::string&, const std::string&, const nlohmann::json&)
{
    std::promise<std::string> promise;
    promise.set_exception(std::make_exception_ptr("OnProduceData(): data channel is not supported"));
    return promise.get_future();
}

void Device::OnTransportClose(mediasoupclient::Producer* producer)
{
    // TODO
    (void)producer;
}

void Device::OnTransportClose(mediasoupclient::Consumer* consumer)
{
    // TODO
    (void)consumer;
}

}