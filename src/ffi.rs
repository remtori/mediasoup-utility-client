use std::os::raw::{c_char, c_int, c_uint, c_void};

pub const MSC_MEDIA_KIND_AUDIO: MscMediaKind = 0;
pub const MSC_MEDIA_KIND_VIDEO: MscMediaKind = 1;
pub type MscMediaKind = c_uint;

#[repr(C)]
pub struct MscDevice {
    _address: u8,
}

#[repr(C)]
pub struct MscTransport {
    _address: u8,
}

#[repr(C)]
pub struct MscProducer {
    producer: *mut c_void,
    source: *mut c_void,
}

#[repr(C)]
pub struct MscConsumer {
    _address: u8,
}

#[repr(C)]
pub struct MscConsumerSink {
    _address: u8,
}

#[repr(C)]
pub struct MscVideoFrame {
    pub data: *mut u8,
    pub width: c_int,
    pub height: c_int,
    pub timestamp: i64,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct MscAudioData {
    pub data: *const c_void,
    pub bits_per_sample: c_int,
    pub sample_rate: c_int,
    pub number_of_channels: c_int,
    pub number_of_frames: c_int,
    pub absolute_capture_timestamp_ms: i64,
}

pub type OnConnect = unsafe extern "C" fn(
    transport: *mut MscTransport,
    user_ptr: *mut c_void,
    dtls_parameters: *const c_char,
);

pub type OnConnectionStateChange = unsafe extern "C" fn(
    transport: *mut MscTransport,
    user_ptr: *mut c_void,
    connection_state: *const c_char,
);

pub type OnProduce = unsafe extern "C" fn(
    transport: *mut MscTransport,
    user_ptr: *mut c_void,
    promise_id: i64,
    kind: MscMediaKind,
    rtp_parameters: *const c_char,
);

pub type OnVideoFrame = unsafe extern "C" fn(this: MscVideoFrame);
pub type OnAudioData = unsafe extern "C" fn(this: MscAudioData);

extern "C" {
    pub fn msc_initialize();
    pub fn msc_cleanup();

    pub fn msc_version() -> *const c_char;
    pub fn msc_free_string(this: *const c_char);

    pub fn msc_alloc_device() -> *mut MscDevice;
    pub fn msc_free_device(this: *mut MscDevice);

    pub fn msc_get_rtp_capabilities(this: *mut MscDevice) -> *const c_char;
    pub fn msc_is_loaded(this: *mut MscDevice) -> bool;
    pub fn msc_load(this: *mut MscDevice, router_rtp_capabilities: *const c_char) -> bool;
    pub fn msc_can_produce(this: *mut MscDevice, kind: MscMediaKind) -> bool;

    pub fn msc_set_user_ptr(this: *mut MscDevice, user_ptr: *mut c_void);
    pub fn msc_set_on_connect_handler(device: *mut MscDevice, handler: OnConnect);
    pub fn msc_set_on_connection_state_change_handler(
        device: *mut MscDevice,
        handler: OnConnectionStateChange,
    );
    pub fn msc_set_on_produce_handler(device: *mut MscDevice, handler: OnProduce);

    pub fn msc_fulfill_producer_id(
        this: *mut MscDevice,
        promise_id: i64,
        producer_id: *const c_char,
    );

    pub fn msc_create_send_transport(
        this: *mut MscDevice,
        id: *const c_char,
        ice_parameters: *const c_char,
        ice_candidates: *const c_char,
        dtls_parameters: *const c_char,
    ) -> *mut MscTransport;
    pub fn msc_create_recv_transport(
        this: *mut MscDevice,
        id: *const c_char,
        ice_parameters: *const c_char,
        ice_candidates: *const c_char,
        dtls_parameters: *const c_char,
    ) -> *mut MscTransport;
    pub fn msc_create_producer(
        this: *mut MscDevice,
        transport: *mut MscTransport,
        encoding_layers: c_int,
        codec_options: *const c_char,
        codec: *const c_char,
    ) -> *mut MscProducer;

    pub fn msc_supply_video(this: *mut MscDevice, producer: *mut MscProducer, data: MscVideoFrame);
    pub fn msc_supply_audio(this: *mut MscDevice, producer: *mut MscProducer, data: MscAudioData);

    pub fn msc_create_consumer(
        this: *mut MscDevice,
        transport: *mut MscTransport,
        id: *const c_char,
        producer_id: *const c_char,
        kind: MscMediaKind,
        rtp_parameters: *const c_char,
    ) -> *mut MscConsumer;
    pub fn msc_add_video_sink(
        this: *mut MscDevice,
        consumer: *mut MscConsumer,
        handler: OnVideoFrame,
    ) -> *mut MscConsumerSink;
    pub fn msc_add_audio_sink(
        this: *mut MscDevice,
        consumer: *mut MscConsumer,
        handler: OnAudioData,
    ) -> *mut MscConsumerSink;
    pub fn msc_remove_video_sink(
        this: *mut MscDevice,
        consumer: *mut MscConsumer,
        handler: *mut MscConsumerSink,
    );
    pub fn msc_remove_audio_sink(
        this: *mut MscDevice,
        consumer: *mut MscConsumer,
        handler: *mut MscConsumerSink,
    );
}
