#pragma once

#ifdef GEN_BINDINGS
#    include <stdbool.h>
#    define EXPORT
#    define NOEXCEPT
#else
#    include <stdint.h>
#    define EXPORT __attribute__((visibility("default")))
#    define NOEXCEPT noexcept
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

EXPORT void msc_initialize(WriteLog) NOEXCEPT;
EXPORT void msc_cleanup() NOEXCEPT;

EXPORT const char* msc_version() NOEXCEPT;
EXPORT void msc_free_string(const char*) NOEXCEPT;

typedef void (*OnConnect)(MscDevice* device, MscTransport* transport, void* ctx, const char* dtls_parameters);
typedef void (*OnConnectionStateChange)(MscDevice* device, MscTransport* transport, void* ctx, const char* connection_state);
typedef void (*OnProduce)(MscDevice* device, MscTransport* transport, void* ctx, int64_t promise_id, MscMediaKind kind, const char* rtp_parameters);

EXPORT MscDevice* msc_alloc_device(
    void* ctx,
    OnConnect on_connect_handler,
    OnConnectionStateChange on_connection_state_change_handler,
    OnProduce on_produce_handler) NOEXCEPT;
EXPORT void msc_free_device(MscDevice*) NOEXCEPT;

EXPORT const char* msc_get_rtp_capabilities(MscDevice*) NOEXCEPT;
EXPORT bool msc_load(MscDevice*, const char* router_rtp_capabilities) NOEXCEPT;
EXPORT bool msc_can_produce(MscDevice*, MscMediaKind kind) NOEXCEPT;

EXPORT void msc_fulfill_producer_id(MscDevice*, int64_t promise_id, const char* producer_id) NOEXCEPT;


EXPORT MscTransport* msc_create_transport(
    MscDevice*,
	bool is_send,
    const char* id,
    const char* ice_parameters,
    const char* ice_candidates,
    const char* dtls_parameters,
	void* transport_ctx) NOEXCEPT;
EXPORT void msc_free_transport(MscTransport*) NOEXCEPT;

EXPORT const char* msc_transport_get_id(MscTransport*) NOEXCEPT;
EXPORT const void* msc_transport_get_ctx(MscTransport*) NOEXCEPT;

EXPORT MscProducer* msc_create_producer(
    MscDevice* device,
    MscTransport* transport,
    int encoding_layers,
    const char* codec_options,
    const char* codec) NOEXCEPT;
EXPORT void msc_free_producer(MscProducer*) NOEXCEPT;
EXPORT void msc_supply_video(MscDevice* device, MscProducer* producer, MscVideoFrame) NOEXCEPT;
EXPORT void msc_supply_audio(MscDevice* device, MscProducer* producer, MscAudioData) NOEXCEPT;

EXPORT MscConsumer* msc_create_consumer(
    MscDevice* device,
    MscTransport* transport,
    const char* id,
    const char* producer_id,
    MscMediaKind kind,
    const char* rtp_parameters) NOEXCEPT;
EXPORT void msc_free_consumer(MscConsumer*) NOEXCEPT;

typedef void (*OnVideoFrame)(void* ctx, MscVideoFrame video_frame);
typedef void (*OnAudioData)(void* ctx, MscAudioData audio_data);

EXPORT MscConsumerSink* msc_add_video_sink(MscConsumer* consumer, void* ctx, OnVideoFrame callback) NOEXCEPT;
EXPORT MscConsumerSink* msc_add_audio_sink(MscConsumer* consumer, void* ctx, OnAudioData callback) NOEXCEPT;
EXPORT void msc_remove_video_sink(MscConsumer* consumer, MscConsumerSink* sink) NOEXCEPT;
EXPORT void msc_remove_audio_sink(MscConsumer* consumer, MscConsumerSink* sink) NOEXCEPT;

#ifdef __cplusplus
};
#endif
