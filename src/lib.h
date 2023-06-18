#pragma once

#ifdef GEN_BINDINGS
#include <stdbool.h>
#define EXPORT
#else
#include <stdint.h>
#define EXPORT __attribute__((visibility("default")))
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

EXPORT void msc_initialize();
EXPORT void msc_cleanup();

EXPORT const char* msc_version();
EXPORT void msc_free_string(const char*);

EXPORT MscDevice* msc_alloc_device();
EXPORT void msc_free_device(MscDevice*);

EXPORT const char* msc_get_rtp_capabilities(MscDevice*);
EXPORT bool msc_is_loaded(MscDevice*);
EXPORT bool msc_load(MscDevice*, const char* router_rtp_capabilities);
EXPORT bool msc_can_produce(MscDevice*, MscMediaKind kind);

typedef void (*OnConnect)(MscDevice*, MscTransport*, const char* dtls_parameters);
typedef void (*OnConnectionStateChange)(MscDevice*, MscTransport*, const char* connection_state);
typedef void (*OnProduce)(MscDevice*, MscTransport*, int64_t promise_id, MscMediaKind kind, const char* rtp_parameters);
typedef void (*OnSendTransportClose)(MscDevice*, MscTransport*, MscProducer*);
typedef void (*OnRecvTransportClose)(MscDevice*, MscTransport*, MscConsumer*);

EXPORT void msc_set_on_connect_handler(MscDevice*, OnConnect handler);
EXPORT void msc_set_on_connection_state_change_handler(MscDevice*, OnConnectionStateChange handler);
EXPORT void msc_set_on_produce_handler(MscDevice*, OnProduce handler);
EXPORT void msc_set_on_send_transport_close_handler(MscDevice*, OnSendTransportClose handler);
EXPORT void msc_set_on_recv_transport_close_handler(MscDevice*, OnRecvTransportClose handler);

EXPORT void msc_fulfill_producer_id(MscDevice*, int64_t promise_id, const char* producer_id);

EXPORT MscTransport* msc_create_send_transport(
        MscDevice*,
        const char* id,
        const char* ice_parameters,
        const char* ice_candidates,
        const char* dtls_parameters
);

EXPORT MscTransport* msc_create_recv_transport(
        MscDevice*,
        const char* id,
        const char* ice_parameters,
        const char* ice_candidates,
        const char* dtls_parameters
);

EXPORT MscProducer* msc_create_producer(MscDevice*, MscTransport*, int encoding_layers, const char* codec_options, const char* codec);
EXPORT void msc_supply_video(MscDevice*, MscProducer*, MscVideoFrame);
EXPORT void msc_supply_audio(MscDevice*, MscProducer*, MscAudioData);

EXPORT MscConsumer* msc_create_consumer(MscDevice*, MscTransport*, const char* id, const char* producer_id, MscMediaKind kind, const char* rtp_parameters);

typedef void (*OnVideoFrame)(MscVideoFrame);
typedef void (*OnAudioData)(MscAudioData);

EXPORT MscConsumerSink* msc_add_video_sink(MscDevice*, MscConsumer*, OnVideoFrame);
EXPORT MscConsumerSink* msc_add_audio_sink(MscDevice*, MscConsumer*, OnAudioData);
EXPORT void msc_remove_video_sink(MscDevice*, MscConsumer*, MscConsumerSink*);
EXPORT void msc_remove_audio_sink(MscDevice*, MscConsumer*, MscConsumerSink*);

#ifdef __cplusplus
};
#endif
