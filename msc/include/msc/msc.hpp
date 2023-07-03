#pragma once

#include <any>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <optional>

#include "./executor.hpp"
#include "./json.hpp"

#ifdef _WIN32
#    ifdef MSC_EXPORTS
#        define EXPORT __declspec(dllexport)
#    else
#        define EXPORT __declspec(dllimport)
#    endif
#else
#    define EXPORT __attribute__((visibility("default")))
#endif

namespace msc {

enum class EXPORT MediaKind {
    Audio,
    Video,
};

enum class EXPORT TransportKind {
    Recv,
    Send,
};

struct EXPORT CreateTransportOptions {
    std::string id;
    nlohmann::json ice_parameters = nullptr;
    nlohmann::json ice_candidates = nullptr;
    nlohmann::json dtls_parameters = nullptr;
    nlohmann::json sctp_parameters = nullptr;
};

class EXPORT DeviceDelegate {
public:
    virtual std::future<CreateTransportOptions> create_server_side_transport(TransportKind kind) noexcept = 0;
    virtual std::future<void> connect_transport(const std::string& transport_id, const nlohmann::json& dtls_parameters) noexcept = 0;

    virtual std::future<std::string> connect_producer(const std::string& transport_id, MediaKind kind, const nlohmann::json& rtp_parameters) noexcept = 0;
    virtual std::future<std::string> connect_data_producer(const std::string& transport_id, const nlohmann::json& sctp_parameters, const std::string& label, const std::string& protocol) noexcept = 0;

    virtual void on_connection_state_change(const std::string& transport_id, const std::string& connection_state) noexcept = 0;
};

struct EXPORT VideoFrame {
    int64_t timestamp_ms;
    int width;
    int height;
    const uint8_t* data_y;
    const uint8_t* data_u;
    const uint8_t* data_v;
    int stride_y;
    int stride_u;
    int stride_v;
};

struct EXPORT AudioData {
    int64_t timestamp_ms;
    int bits_per_sample;
    int sample_rate;
    int number_of_channels;
    int number_of_frames;
    const void* data;
};

class EXPORT VideoConsumer {
public:
    virtual void on_video_frame(const VideoFrame&) = 0;
    virtual void on_close() = 0;
};

class EXPORT AudioConsumer {
public:
    virtual void on_audio_consumer(const AudioData&) = 0;
    virtual void on_close() = 0;
};

class EXPORT Device {
public:
    static std::shared_ptr<Device> create(std::shared_ptr<Executor> executor, std::shared_ptr<DeviceDelegate> delegate) noexcept;

    virtual ~Device() { }

    virtual bool load(const nlohmann::json& rtp_capabilities) noexcept = 0;
    virtual const nlohmann::json& rtp_capabilities() const noexcept = 0;

    virtual bool can_produce(MediaKind) noexcept = 0;

    virtual void ensure_transport(TransportKind kind) noexcept = 0;

    virtual void create_video_sink(const std::string& id, const std::string& producer_id, const nlohmann::json& rtp_parameters, std::shared_ptr<VideoConsumer> consumer) noexcept = 0;
    virtual void create_audio_sink(const std::string& id, const std::string& producer_id, const nlohmann::json& rtp_parameters, std::shared_ptr<AudioConsumer> consumer) noexcept = 0;

    virtual void close_video_sink(const std::shared_ptr<VideoConsumer>&) noexcept = 0;
    virtual void close_audio_sink(const std::shared_ptr<AudioConsumer>&) noexcept = 0;
};

}
