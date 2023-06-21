#pragma once

#ifdef GEN_BINDINGS
#    include <stdbool.h>
#    define EXPORT
#else
#    include <stdint.h>
#    define EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef GEN_BINDINGS
typedef unsigned char uint8_t;
typedef unsigned long long int64_t;
#endif

typedef enum MscMediaKind {
    Audio = 0,
    Video = 1,
} MscMediaKind;

typedef struct MscDevice {
    uint8_t _address;
} MscDevice;

typedef struct MscTransport {
    uint8_t _address;
} MscTransport;

typedef struct MscProducer {
    void* producer;
    void* source;
} MscProducer;

typedef struct MscConsumer {
    uint8_t _address;
} MscConsumer;

typedef struct MscConsumerSink {
    uint8_t _address;
} MscConsumerSink;

typedef struct MscVideoFrame {
    uint8_t* data;
    int width;
    int height;
    int64_t timestamp;
} MscVideoFrame;

typedef struct MscAudioData {
    const void* data;
    int bits_per_sample;
    int sample_rate;
    int number_of_channels;
    int number_of_frames;
    int64_t absolute_capture_timestamp_ms;
} MscAudioData;

typedef void (*WriteLog)(const char*, int);

EXPORT void msc_initialize(WriteLog) noexcept;
EXPORT void msc_cleanup() noexcept;

EXPORT const char* msc_version() noexcept;
EXPORT void msc_free_string(const char*) noexcept;

EXPORT MscDevice* msc_alloc_device() noexcept;
EXPORT void msc_free_device(MscDevice*) noexcept;

EXPORT const char* msc_get_rtp_capabilities(MscDevice*) noexcept;
EXPORT bool msc_is_loaded(MscDevice*) noexcept;
EXPORT bool msc_load(MscDevice*, const char* router_rtp_capabilities) noexcept;
EXPORT bool msc_can_produce(MscDevice*, MscMediaKind kind) noexcept;

typedef void (*OnConnect)(::MscTransport* transport, void* user_ptr, const char* dtls_parameters);
typedef void (*OnConnectionStateChange)(::MscTransport* transport, void* user_ptr, const char* connection_state);
typedef void (*OnProduce)(::MscTransport* transport, void* user_ptr, int64_t promise_id, MscMediaKind kind, const char* rtp_parameters);

EXPORT void msc_set_user_ptr(MscDevice*, void*) noexcept;
EXPORT void msc_set_on_connect_handler(MscDevice*, OnConnect handler) noexcept;
EXPORT void msc_set_on_connection_state_change_handler(MscDevice*, OnConnectionStateChange handler) noexcept;
EXPORT void msc_set_on_produce_handler(MscDevice*, OnProduce handler) noexcept;

EXPORT void msc_fulfill_producer_id(MscDevice*, int64_t promise_id, const char* producer_id) noexcept;

EXPORT const char* msc_transport_get_id(MscTransport*) noexcept;

EXPORT MscTransport* msc_create_send_transport(
    MscDevice*,
    const char* id,
    const char* ice_parameters,
    const char* ice_candidates,
    const char* dtls_parameters) noexcept;

EXPORT MscTransport* msc_create_recv_transport(
    MscDevice*,
    const char* id,
    const char* ice_parameters,
    const char* ice_candidates,
    const char* dtls_parameters) noexcept;

EXPORT MscProducer* msc_create_producer(MscDevice*, MscTransport*, int encoding_layers, const char* codec_options, const char* codec) noexcept;
EXPORT void msc_supply_video(MscDevice*, MscProducer*, MscVideoFrame) noexcept;
EXPORT void msc_supply_audio(MscDevice*, MscProducer*, MscAudioData) noexcept;

EXPORT MscConsumer* msc_create_consumer(MscDevice*, MscTransport*, const char* id, const char* producer_id, MscMediaKind kind, const char* rtp_parameters) noexcept;
EXPORT void msc_free_consumer(MscConsumer*) noexcept;

typedef void (*OnVideoFrame)(void*, MscVideoFrame);
typedef void (*OnAudioData)(void*, MscAudioData);

EXPORT MscConsumerSink* msc_add_video_sink(MscConsumer*, void* user_ptr, OnVideoFrame) noexcept;
EXPORT MscConsumerSink* msc_add_audio_sink(MscConsumer*, void* user_ptr, OnAudioData) noexcept;
EXPORT void msc_remove_video_sink(MscConsumer*, MscConsumerSink*) noexcept;
EXPORT void msc_remove_audio_sink(MscConsumer*, MscConsumerSink*) noexcept;

#ifdef __cplusplus
};
#endif
