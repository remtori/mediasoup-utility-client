#pragma once

#ifdef GEN_BINDINGS
#include <stdbool.h>
#else
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef GEN_BINDINGS
typedef unsigned char uint8_t;
typedef unsigned long long int64_t;
#endif

typedef enum MediaKind {
    Audio = 0,
    Video = 1,
} MediaKind;

typedef struct Device {
    uint8_t _address;
} Device;

typedef struct Transport {
    uint8_t _address;
} Transport;

typedef struct Producer {
    void* producer;
    void* source;
} Producer;

typedef struct Consumer {
    uint8_t _address;
} Consumer;

typedef struct ConsumerSink {
    uint8_t _address;
} ConsumerSink;

typedef struct VideoFrame {
    uint8_t* data;
    int width;
    int height;
    int64_t timestamp;
} VideoFrame;

typedef struct AudioData {
    const void* data;
    int bits_per_sample;
    int sample_rate;
    int number_of_channels;
    int number_of_frames;
    int64_t absolute_capture_timestamp_ms;
} AudioData;

void initialize();
void cleanup();

const char* version();
void free_string(const char*);

Device* alloc_device();
void free_device(Device*);

const char* get_rtp_capabilities(Device*);
bool is_loaded(Device*);
bool load(Device*, const char* router_rtp_capabilities);
bool can_produce(Device*, MediaKind kind);

typedef void (*OnConnect)(Device*, Transport*, const char* dtls_parameters);
typedef void (*OnConnectionStateChange)(Device*, Transport*, const char* connection_state);
typedef void (*OnProduce)(Device*, Transport*, int64_t promise_id, MediaKind kind, const char* rtp_parameters);
typedef void (*OnSendTransportClose)(Device*, Transport*, Producer*);
typedef void (*OnRecvTransportClose)(Device*, Transport*, Consumer*);

void set_on_connect_handler(Device*, OnConnect handler);
void set_on_connection_state_change_handler(Device*, OnConnectionStateChange handler);
void set_on_produce_handler(Device*, OnProduce handler);
void set_on_send_transport_close_handler(Device*, OnSendTransportClose handler);
void set_on_recv_transport_close_handler(Device*, OnRecvTransportClose handler);

void fulfill_producer_id(Device*, int64_t promise_id, const char* producer_id);

Transport* create_send_transport(
    Device*,
    const char* id,
    const char* ice_parameters,
    const char* ice_candidates,
    const char* dtls_parameters
);

Transport* create_recv_transport(
    Device*,
    const char* id,
    const char* ice_parameters,
    const char* ice_candidates,
    const char* dtls_parameters
);

Producer* create_producer(Device*, Transport*, int encoding_layers, const char* codec_options, const char* codec);
void supply_video(Device*, Producer*, VideoFrame);
void supply_audio(Device*, Producer*, AudioData);

Consumer* create_consumer(Device*, Transport*, const char* id, const char* producer_id, MediaKind kind, const char* rtp_parameters);

typedef void (*OnVideoFrame)(VideoFrame);
typedef void (*OnAudioData)(AudioData);

ConsumerSink* add_video_sink(Device*, Consumer*, OnVideoFrame);
ConsumerSink* add_audio_sink(Device*, Consumer*, OnAudioData);
void remove_video_sink(Device*, Consumer*, ConsumerSink*);
void remove_audio_sink(Device*, Consumer*, ConsumerSink*);

#ifdef __cplusplus
};
#endif
