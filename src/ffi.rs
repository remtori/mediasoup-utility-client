pub const MscMediaKind_Audio: MscMediaKind = 0;
pub const MscMediaKind_Video: MscMediaKind = 1;
pub type MscMediaKind = ::std::os::raw::c_uint;

#[repr(C)]
pub struct MscDevice {
    pub _address: u8,
}

#[repr(C)]
pub struct MscTransport {
    pub _address: u8,
}

#[repr(C)]
pub struct MscProducer {
    pub producer: *mut ::std::os::raw::c_void,
    pub source: *mut ::std::os::raw::c_void,
}

#[repr(C)]
pub struct MscConsumer {
    pub _address: u8,
}

#[repr(C)]
pub struct MscConsumerSink {
    pub _address: u8,
}

#[repr(C)]
pub struct MscVideoFrame {
    pub data: *mut u8,
    pub width: ::std::os::raw::c_int,
    pub height: ::std::os::raw::c_int,
    pub timestamp: i64,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct MscAudioData {
    pub data: *const ::std::os::raw::c_void,
    pub bits_per_sample: ::std::os::raw::c_int,
    pub sample_rate: ::std::os::raw::c_int,
    pub number_of_channels: ::std::os::raw::c_int,
    pub number_of_frames: ::std::os::raw::c_int,
    pub absolute_capture_timestamp_ms: i64,
}

pub type OnConnect = ::std::option::Option<
    unsafe extern "C" fn(
        arg1: *mut MscDevice,
        arg2: *mut MscTransport,
        dtls_parameters: *const ::std::os::raw::c_char,
    ),
>;

pub type OnConnectionStateChange = ::std::option::Option<
    unsafe extern "C" fn(
        arg1: *mut MscDevice,
        arg2: *mut MscTransport,
        connection_state: *const ::std::os::raw::c_char,
    ),
>;

pub type OnProduce = ::std::option::Option<
    unsafe extern "C" fn(
        arg1: *mut MscDevice,
        arg2: *mut MscTransport,
        promise_id: i64,
        kind: MscMediaKind,
        rtp_parameters: *const ::std::os::raw::c_char,
    ),
>;

pub type OnSendTransportClose = ::std::option::Option<
    unsafe extern "C" fn(arg1: *mut MscDevice, arg2: *mut MscTransport, arg3: *mut MscProducer),
>;

pub type OnRecvTransportClose = ::std::option::Option<
    unsafe extern "C" fn(arg1: *mut MscDevice, arg2: *mut MscTransport, arg3: *mut MscConsumer),
>;

pub type OnVideoFrame = ::std::option::Option<unsafe extern "C" fn(arg1: MscVideoFrame)>;
pub type OnAudioData = ::std::option::Option<unsafe extern "C" fn(arg1: MscAudioData)>;

extern "C" {
    pub fn msc_initialize();
    pub fn msc_cleanup();

    pub fn msc_version() -> *const ::std::os::raw::c_char;
    pub fn msc_free_string(arg1: *const ::std::os::raw::c_char);

    pub fn msc_alloc_device() -> *mut MscDevice;
    pub fn msc_free_device(arg1: *mut MscDevice);

    pub fn msc_get_rtp_capabilities(arg1: *mut MscDevice) -> *const ::std::os::raw::c_char;
    pub fn msc_is_loaded(arg1: *mut MscDevice) -> bool;
    pub fn msc_load(
        arg1: *mut MscDevice,
        router_rtp_capabilities: *const ::std::os::raw::c_char,
    ) -> bool;
    pub fn msc_can_produce(arg1: *mut MscDevice, kind: MscMediaKind) -> bool;

    pub fn msc_set_on_connect_handler(arg1: *mut MscDevice, handler: OnConnect);
    pub fn msc_set_on_connection_state_change_handler(
        arg1: *mut MscDevice,
        handler: OnConnectionStateChange,
    );
    pub fn msc_set_on_produce_handler(arg1: *mut MscDevice, handler: OnProduce);
    pub fn msc_set_on_send_transport_close_handler(
        arg1: *mut MscDevice,
        handler: OnSendTransportClose,
    );
    pub fn msc_set_on_recv_transport_close_handler(
        arg1: *mut MscDevice,
        handler: OnRecvTransportClose,
    );

    pub fn msc_fulfill_producer_id(
        arg1: *mut MscDevice,
        promise_id: i64,
        producer_id: *const ::std::os::raw::c_char,
    );

    pub fn msc_create_send_transport(
        arg1: *mut MscDevice,
        id: *const ::std::os::raw::c_char,
        ice_parameters: *const ::std::os::raw::c_char,
        ice_candidates: *const ::std::os::raw::c_char,
        dtls_parameters: *const ::std::os::raw::c_char,
    ) -> *mut MscTransport;
    pub fn msc_create_recv_transport(
        arg1: *mut MscDevice,
        id: *const ::std::os::raw::c_char,
        ice_parameters: *const ::std::os::raw::c_char,
        ice_candidates: *const ::std::os::raw::c_char,
        dtls_parameters: *const ::std::os::raw::c_char,
    ) -> *mut MscTransport;
    pub fn msc_create_producer(
        arg1: *mut MscDevice,
        arg2: *mut MscTransport,
        encoding_layers: ::std::os::raw::c_int,
        codec_options: *const ::std::os::raw::c_char,
        codec: *const ::std::os::raw::c_char,
    ) -> *mut MscProducer;

    pub fn msc_supply_video(arg1: *mut MscDevice, arg2: *mut MscProducer, arg3: MscVideoFrame);
    pub fn msc_supply_audio(arg1: *mut MscDevice, arg2: *mut MscProducer, arg3: MscAudioData);

    pub fn msc_create_consumer(
        arg1: *mut MscDevice,
        arg2: *mut MscTransport,
        id: *const ::std::os::raw::c_char,
        producer_id: *const ::std::os::raw::c_char,
        kind: MscMediaKind,
        rtp_parameters: *const ::std::os::raw::c_char,
    ) -> *mut MscConsumer;
    pub fn msc_add_video_sink(
        arg1: *mut MscDevice,
        arg2: *mut MscConsumer,
        arg3: OnVideoFrame,
    ) -> *mut MscConsumerSink;
    pub fn msc_add_audio_sink(
        arg1: *mut MscDevice,
        arg2: *mut MscConsumer,
        arg3: OnAudioData,
    ) -> *mut MscConsumerSink;
    pub fn msc_remove_video_sink(
        arg1: *mut MscDevice,
        arg2: *mut MscConsumer,
        arg3: *mut MscConsumerSink,
    );
    pub fn msc_remove_audio_sink(
        arg1: *mut MscDevice,
        arg2: *mut MscConsumer,
        arg3: *mut MscConsumerSink,
    );
}
