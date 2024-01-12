#pragma once

#include <any>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <span>

#include <common/executor.hpp>
#include <common/json.hpp>

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

EXPORT void initialize();
EXPORT int64_t rtc_timestamp_ms();

class EXPORT PeerConnectionFactoryTuple {
public:
    virtual ~PeerConnectionFactoryTuple() = default;
};

EXPORT std::shared_ptr<PeerConnectionFactoryTuple> create_peer_connection_factory();

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
    virtual ~DeviceDelegate() { }

    virtual std::future<CreateTransportOptions> create_server_side_transport(TransportKind kind, const nlohmann::json& rtp_capabilities) noexcept = 0;
    virtual std::future<void> connect_transport(TransportKind kind, const std::string& transport_id, const nlohmann::json& dtls_parameters) noexcept = 0;

    virtual std::future<std::string> connect_producer(const std::string& transport_id, MediaKind kind, const nlohmann::json& rtp_parameters) noexcept
    {
        std::promise<std::string> ret;
        ret.set_exception(std::make_exception_ptr(std::runtime_error("not implemented")));
        return ret.get_future();
    }

    virtual std::future<std::string> connect_data_producer(const std::string& transport_id, const nlohmann::json& sctp_parameters, const std::string& label, const std::string& protocol) noexcept
    {
        std::promise<std::string> ret;
        ret.set_exception(std::make_exception_ptr(std::runtime_error("not implemented")));
        return ret.get_future();
    }

    virtual void on_connection_state_change(TransportKind kind, const std::string& transport_id, const std::string& connection_state) noexcept
    {
        (void)kind;
        (void)transport_id;
        (void)connection_state;
    }
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

struct EXPORT MutableVideoFrame {
    int64_t timestamp_ms;
    int width;
    int height;
    uint8_t* data_y;
    uint8_t* data_u;
    uint8_t* data_v;
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

struct EXPORT MutableAudioData {
    int64_t timestamp_ms;
    int bits_per_sample;
    int sample_rate;
    int number_of_channels;
    int number_of_frames;
    void* data;
};

class EXPORT VideoConsumer {
public:
    virtual ~VideoConsumer() = default;

    virtual void on_video_frame(const VideoFrame&) = 0;
    virtual void on_close() = 0;
};

class EXPORT AudioConsumer {
public:
    virtual ~AudioConsumer() = default;

    virtual void on_audio_data(const AudioData&) = 0;
    virtual void on_close() = 0;
};

class EXPORT DataConsumer {
public:
    virtual ~DataConsumer() = default;

    virtual void on_data(std::span<const uint8_t>) = 0;
};

class DummyVideoConsumer : public VideoConsumer {
public:
    DummyVideoConsumer() = default;
    ~DummyVideoConsumer() = default;

private:
    void on_video_frame(const VideoFrame&) final { }
    void on_close() final { }
};

class DummyAudioConsumer : public AudioConsumer {
public:
    DummyAudioConsumer() = default;
    ~DummyAudioConsumer() = default;

private:
    void on_audio_data(const AudioData&) final { }
    void on_close() final { }
};

class EXPORT VideoSender {
public:
    virtual ~VideoSender() = default;

    virtual bool is_closed() = 0;

    virtual std::optional<MutableVideoFrame> adapt_frame(
        int sourceWidth, int sourceHeight, int64_t timestamp,
        int* adapted_width, int* adapted_height,
        int* crop_width, int* crop_height,
        int* crop_x, int* crop_y)
        = 0;

    virtual void send_video_frame(MutableVideoFrame) = 0;
};

class EXPORT AudioSender {
public:
    virtual ~AudioSender() = default;

    virtual bool is_closed() = 0;

    virtual void send_audio_data(const MutableAudioData&) = 0;
};

class EXPORT DataSender {
public:
    virtual ~DataSender() = default;

    virtual bool is_closed() = 0;
    virtual uint64_t buffered_amount() = 0;
    virtual void send_data(std::span<const uint8_t>) = 0;
};

struct EXPORT ConsumerOptions {
    const std::string& consumer_id;
    const std::string& producer_id;
    const nlohmann::json& rtp_parameters;
};

struct EXPORT ProducerOptions {
    const nlohmann::json& encodings;
    const nlohmann::json& codecOptions;
    const nlohmann::json& codec;
};

class EXPORT Device {
protected:
    Device() {};

public:
    static std::unique_ptr<Device> create(DeviceDelegate* delegate, std::shared_ptr<PeerConnectionFactoryTuple> peer_connection_factory_tuple = nullptr) noexcept;

    virtual ~Device() {};

    virtual bool load(const nlohmann::json& rtp_capabilities) noexcept = 0;
    virtual const nlohmann::json& rtp_capabilities() const noexcept = 0;

    virtual bool can_produce(MediaKind) noexcept = 0;
    virtual void stop() noexcept = 0;

    virtual void ensure_transport(TransportKind kind) noexcept = 0;

    virtual void create_video_sink(
        const ConsumerOptions&,
        std::shared_ptr<VideoConsumer> consumer = nullptr) noexcept
        = 0;
    virtual void create_audio_sink(
        const ConsumerOptions&,
        std::shared_ptr<AudioConsumer> consumer = nullptr) noexcept
        = 0;
    virtual void create_data_sink(
        const std::string& consumer_id,
        const std::string& producer_id,
        uint16_t stream_id,
        const std::string& label,
        const std::string& protocol,
        std::shared_ptr<DataConsumer> = nullptr) noexcept
        = 0;

    virtual void close_video_sink(const std::shared_ptr<VideoConsumer>&) noexcept = 0;
    virtual void close_audio_sink(const std::shared_ptr<AudioConsumer>&) noexcept = 0;
    virtual void close_data_sink(const std::shared_ptr<DataConsumer>&) noexcept = 0;

    virtual std::shared_ptr<VideoSender> create_video_source(const ProducerOptions&) noexcept = 0;
    virtual std::shared_ptr<AudioSender> create_audio_source(const ProducerOptions&) noexcept = 0;

    virtual std::shared_ptr<DataSender> create_data_source(
        const std::string& label,
        const std::string& protocol,
        bool ordered,
        int maxRetransmits,
        int maxPacketLifeTime) noexcept
        = 0;

    virtual std::shared_ptr<void> re_encode(MediaKind, const ConsumerOptions&, const ProducerOptions&) noexcept = 0;
};

}
