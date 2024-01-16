#include <common/executor.hpp>
#include <common/logger.hpp>
#include <msc/msc.hpp>
#include <net/http_client.hpp>
#include <net/protoo.hpp>

#include "./channel.hpp"

namespace msc {

inline void from_json(const nlohmann::json& nlohmann_json_j, ConsumerOptions& nlohmann_json_t)
{
    nlohmann_json_j.at("consumerId").get_to(nlohmann_json_t.consumer_id);
    nlohmann_json_j.at("producerId").get_to(nlohmann_json_t.producer_id);
    nlohmann_json_j.at("rtpParameters").get_to(nlohmann_json_t.rtp_parameters);
}

inline void from_json(const nlohmann::json& nlohmann_json_j, ProducerOptions& nlohmann_json_t)
{
    nlohmann_json_j.at("encodings").get_to(nlohmann_json_t.encodings);
    nlohmann_json_j.at("codecOptions").get_to(nlohmann_json_t.codec_options);
    nlohmann_json_j.at("codec").get_to(nlohmann_json_t.codec);
}

}

struct ReEncoderStream {
    bool is_audio;
    msc::ConsumerOptions consumer_options;
    msc::ProducerOptions producer_options;

    friend void from_json(const nlohmann::json& nlohmann_json_j, ReEncoderStream& nlohmann_json_t)
    {
        nlohmann_json_j.at("isAudio").get_to(nlohmann_json_t.is_audio);
        nlohmann_json_j.at("consumerOptions").get_to(nlohmann_json_t.consumer_options);
        nlohmann_json_j.at("producerOptions").get_to(nlohmann_json_t.producer_options);
    }
};

struct ReEncoderInput {
    nlohmann::json router_rtp_capabilities;
    nlohmann::json send_transport_option;
    nlohmann::json recv_transport_option;
    std::vector<ReEncoderStream> streams;

    friend void from_json(const nlohmann::json& nlohmann_json_j, ReEncoderInput& nlohmann_json_t)
    {
        nlohmann_json_j.at("routerRtpCapabilities").get_to(nlohmann_json_t.router_rtp_capabilities);
        nlohmann_json_j.at("sendTransport").get_to(nlohmann_json_t.send_transport_option);
        nlohmann_json_j.at("recvTransport").get_to(nlohmann_json_t.recv_transport_option);
        nlohmann_json_j.at("streams").get_to(nlohmann_json_t.streams);
    }
};

class ReEncoder : public msc::DeviceDelegate {
private:
    std::shared_ptr<cm::Executor> m_executor;
    std::unique_ptr<msc::Device> m_device;
    std::vector<std::shared_ptr<void>> m_reencoder_streams {};
    ReEncoderInput m_input {};
    std::shared_ptr<Channel> m_channel { nullptr };

public:
    ReEncoder(std::shared_ptr<cm::Executor> executor, std::shared_ptr<msc::PeerConnectionFactoryTuple> peer_connection_factory)
        : m_executor(std::move(executor))
    {
        m_device = msc::Device::create(this, std::move(peer_connection_factory));
    }

    ~ReEncoder()
    {
        close();
    }

    std::shared_ptr<cm::Executor> executor() const noexcept
    {
        return m_executor;
    }

    void reencode(ReEncoderInput input, std::shared_ptr<Channel> channel)
    {
        m_input = std::move(input);
        m_channel = std::move(channel);

        m_executor->push_task([this]() {
            m_device->load(m_input.router_rtp_capabilities);

            for (auto stream : m_input.streams) {
                try {
                    auto reencoder = m_device->re_encode(stream.is_audio ? msc::MediaKind::Audio : msc::MediaKind::Video, stream.consumer_options, stream.producer_options);
                    m_reencoder_streams.push_back(reencoder);
                } catch (const std::exception& e) {
                    cm::log("ReEncodeError: {}", e.what());
                }
            }
        });
    }

    void close()
    {
        m_executor->push_task([this]() {
            m_device->stop();
            m_reencoder_streams.clear();
        });
    }

private:
    msc::CreateTransportOptions create_server_side_transport(msc::TransportKind kind, const nlohmann::json& rtp_capabilities) noexcept override
    {
        (void)rtp_capabilities;
        auto info = kind == msc::TransportKind::Send ? m_input.send_transport_option : m_input.recv_transport_option;

        msc::CreateTransportOptions options;
        options.id = info.at("transportId").get<std::string>();
        options.ice_parameters = info.value("iceParameters", nlohmann::json());
        options.ice_candidates = info.value("iceCandidates", nlohmann::json());
        options.dtls_parameters = info.value("dtlsParameters", nlohmann::json());
        options.sctp_parameters = info.value("sctpParameters", nlohmann::json());

        return options;
    }

    void connect_transport(msc::TransportKind, const std::string& transport_id, const nlohmann::json& dtls_parameters) noexcept override
    {
        m_channel->request("connectTransport", {
                                                   { "transportId", transport_id },
                                                   { "dtlsParameters", dtls_parameters },
                                               });
    }

    std::string connect_producer(const std::string& transport_id, msc::MediaKind kind, const nlohmann::json& rtp_parameters) noexcept override
    {
        auto info = m_channel->request("connectProducer", {
                                                              { "kind", kind == msc::MediaKind::Audio ? "audio" : "video" },
                                                              { "rtpParameters", rtp_parameters },
                                                          });

        return info.at("producerId").get<std::string>();
    }
};

int main(int argc, const char** argv)
{
    cm::init_logger("reencoder");
    msc::initialize();
    auto peer_connection_factory = msc::create_peer_connection_factory();
    std::vector<std::shared_ptr<ReEncoder>> reencoders;

    auto channel = std::make_shared<Channel>();
    channel->on_request = [&](Channel::Request request) {
        if (request.method == "reencode") {
            auto input = request.data.get<ReEncoderInput>();
            auto reencoder = std::make_shared<ReEncoder>(std::make_shared<cm::Executor>(1), peer_connection_factory);
            reencoder->reencode(std::move(input), channel);
            reencoders.push_back(reencoder);

            channel->response(request, Result::Ok, {
                                                       { "code", 200 },
                                                       { "id", std::bit_cast<std::intptr_t>(reencoder.get()) },
                                                   });
            return;
        }

        channel->response(request, Result::Error, { { "code", 501 } });
    };
    channel->on_notify = [&](std::string method, nlohmann::json data) {
        if (method == "close") {
            auto id = data.at("id").get<std::intptr_t>();
            for (auto it = reencoders.begin(); it != reencoders.end(); ++it) {
                auto reencoder = *it;
                if (std::bit_cast<std::intptr_t>(reencoder.get()) != id) {
                    continue;
                }

                reencoder->close();
                reencoders.erase(it);
                reencoder->executor()->push_task([](std::shared_ptr<ReEncoder> reencoder) {
                    reencoder.reset();
                },
                    std::move(reencoder));

                break;
            }

            channel->notify("close", { { "id", id } });
            return;
        }

        channel->notify("error", { { "code", 501 } });
    };
    channel->start();

    return 0;
}
