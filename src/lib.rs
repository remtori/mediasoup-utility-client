use std::{
    cell::Cell,
    ffi::{CStr, CString},
    marker::PhantomData,
    os::raw::{c_char, c_void},
    sync::Arc,
};

pub use crate::types::*;
use ffi::*;

pub mod ffi;
mod types;

pub fn initialize() {
    unsafe extern "C" fn print_fn(str: *const c_char, length: i32) {
        let bytes = std::slice::from_raw_parts(str as *const u8, length as usize + 1);
        let str = CStr::from_bytes_with_nul(bytes).unwrap();
        println!("[MSC FFI] {}", str.to_string_lossy());
    }

    unsafe { msc_initialize(Some(print_fn)) };
}

pub fn cleanup() {
    unsafe { msc_cleanup() };
}

pub fn version() -> &'static str {
    use once_cell::sync::OnceCell;

    static VERSION: OnceCell<String> = OnceCell::new();

    VERSION.get_or_init(|| {
        let version = unsafe { CStr::from_ptr(msc_version()) };
        let out = version.to_string_lossy().to_string();
        unsafe { msc_free_string(version.as_ptr()) };
        out
    })
}

pub trait Signaling: Send + Sync {
    fn connect_transport(&mut self, transport_id: &str, dtls_parameters: &DtlsParameters);

    fn create_producer(
        &mut self,
        transport_id: &str,
        kind: MediaKind,
        rtp_parameters: &RtpParameters,
    ) -> ProducerId;

    fn on_connection_state_change(&mut self, transport_id: &str, connection_state: &str);
}

pub struct None;
pub struct Sender;
pub struct Receiver;

struct DeviceHandle(*mut MscDevice);

impl Drop for DeviceHandle {
    fn drop(&mut self) {
        unsafe { msc_free_device(self.0) }
    }
}

pub struct Device<State> {
    device: Arc<DeviceHandle>,
    send_transport: Cell<*mut MscTransport>,
    recv_transport: Cell<*mut MscTransport>,
    signaling: Box<dyn Signaling>,
    _state: PhantomData<State>,
}

impl<S> std::fmt::Debug for Device<S> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Device")
            .field("device", &self.device.0)
            .field("send_transport", &self.send_transport)
            .field("recv_transport", &self.recv_transport)
            .finish()
    }
}

// unsafe impl Send for Inner {}
// unsafe impl Sync for Inner {}

impl Device<None> {
    pub fn new<T: 'static + Signaling>(signaling: T) -> Device<None> {
        unsafe extern "C" fn on_connect_handler<T: Signaling>(
            _device: *mut MscDevice,
            transport: *mut MscTransport,
            ctx: *mut c_void,
            dtls_parameters: *const c_char,
        ) {
            let transport_id = CStr::from_ptr(msc_transport_get_id(transport));
            let dtls_parameters: DtlsParameters =
                serde_json::from_slice(CStr::from_ptr(dtls_parameters).to_bytes()).unwrap();

            let signaling = &mut *(ctx as *mut T);
            signaling.connect_transport(transport_id.to_str().unwrap(), &dtls_parameters);
        }

        unsafe extern "C" fn on_connection_state_change_handler<T: Signaling>(
            _device: *mut MscDevice,
            transport: *mut MscTransport,
            ctx: *mut c_void,
            connection_state: *const c_char,
        ) {
            let transport_id = CStr::from_ptr(msc_transport_get_id(transport));
            let connection_state = CStr::from_ptr(connection_state);

            let signaling = &mut *(ctx as *mut T);
            signaling.on_connection_state_change(
                transport_id.to_str().unwrap(),
                connection_state.to_str().unwrap(),
            );
        }

        unsafe extern "C" fn on_produce_handler<T: Signaling>(
            device: *mut MscDevice,
            transport: *mut MscTransport,
            ctx: *mut c_void,
            promise_id: i64,
            kind: MscMediaKind,
            rtp_parameters: *const c_char,
        ) {
            let transport_id = CStr::from_ptr(msc_transport_get_id(transport));
            let rtp_parameters: RtpParameters =
                serde_json::from_slice(CStr::from_ptr(rtp_parameters).to_bytes()).unwrap();

            let signaling = &mut *(ctx as *mut T);
            let producer_id = signaling.create_producer(
                transport_id.to_str().unwrap(),
                #[allow(non_upper_case_globals)]
                match kind {
                    MscMediaKind_Audio => MediaKind::Audio,
                    MscMediaKind_Video => MediaKind::Video,
                    _ => unreachable!(),
                },
                &rtp_parameters,
            );

            let producer_id = CString::new(producer_id.0).unwrap();
            msc_fulfill_producer_id(device, promise_id, producer_id.as_ptr());
        }

        let signaling = Box::new(signaling);
        let device = unsafe {
            msc_alloc_device(
                signaling.as_ref() as *const T as *mut c_void,
                Some(on_connect_handler::<T>),
                Some(on_connection_state_change_handler::<T>),
                Some(on_produce_handler::<T>),
            )
        };

        Device {
            device: Arc::new(DeviceHandle(device)),
            send_transport: Cell::new(std::ptr::null_mut()),
            recv_transport: Cell::new(std::ptr::null_mut()),
            signaling,
            _state: PhantomData,
        }
    }
}

impl<S> Device<S> {
    pub fn load(&mut self, rtp_capabilities: &RtpCapabilities) {
        let rtp_capabilities = serde_json::to_vec(rtp_capabilities).unwrap();
        self.load_unchecked(rtp_capabilities)
    }

    pub fn load_unchecked(&mut self, rtp_capabilities: impl Into<Vec<u8>>) {
        let rtp_capabilities = CString::new(rtp_capabilities.into()).unwrap();
        unsafe { msc_load(self.device.0, rtp_capabilities.as_ptr()) };
    }

    pub fn is_loaded(&self) -> bool {
        unsafe { msc_is_loaded(self.device.0) }
    }

    pub fn can_produce(&self, kind: MediaKind) -> bool {
        unsafe { msc_can_produce(self.device.0, kind as u32) }
    }

    pub fn rtp_capabilities(&self) -> RtpCapabilities {
        let caps = unsafe { CStr::from_ptr(msc_get_rtp_capabilities(self.device.0)) };
        let ret = serde_json::from_slice(caps.to_bytes()).unwrap();
        unsafe { msc_free_string(caps.as_ptr()) };
        ret
    }

    pub fn rtp_capabilities_unchecked(&self) -> Vec<u8> {
        let caps = unsafe { CStr::from_ptr(msc_get_rtp_capabilities(self.device.0)) };
        let ret = caps.to_bytes().to_vec();
        unsafe { msc_free_string(caps.as_ptr()) };
        ret
    }
}

impl Device<None> {
    pub fn create_send_transport(
        self,
        transport_id: String,
        ice_parameters: &IceParameters,
        ice_candidates: &Vec<IceCandidate>,
        dtls_parameters: &DtlsParameters,
    ) -> Device<Sender> {
        let ice_parameters = serde_json::to_string(ice_parameters).unwrap();
        let ice_candidates = serde_json::to_string(ice_candidates).unwrap();
        let dtls_parameters = serde_json::to_string(dtls_parameters).unwrap();

        self.create_send_transport_unchecked(
            transport_id,
            ice_parameters,
            ice_candidates,
            dtls_parameters,
        )
    }

    pub fn create_send_transport_unchecked(
        self,
        transport_id: impl Into<Vec<u8>>,
        ice_parameters: impl Into<Vec<u8>>,
        ice_candidates: impl Into<Vec<u8>>,
        dtls_parameters: impl Into<Vec<u8>>,
    ) -> Device<Sender> {
        let transport_id = CString::new(transport_id).unwrap();
        let ice_parameters = CString::new(ice_parameters).unwrap();
        let ice_candidates = CString::new(ice_candidates).unwrap();
        let dtls_parameters = CString::new(dtls_parameters).unwrap();

        let transport = unsafe {
            msc_create_send_transport(
                self.device.0,
                transport_id.as_ptr(),
                ice_parameters.as_ptr(),
                ice_candidates.as_ptr(),
                dtls_parameters.as_ptr(),
            )
        };

        self.send_transport.set(transport);

        Device {
            device: self.device,
            recv_transport: self.recv_transport,
            send_transport: self.send_transport,
            signaling: self.signaling,
            _state: PhantomData,
        }
    }

    pub fn create_recv_transport(
        self,
        transport_id: String,
        ice_parameters: &IceParameters,
        ice_candidates: &Vec<IceCandidate>,
        dtls_parameters: &DtlsParameters,
    ) -> Device<Receiver> {
        let ice_parameters = serde_json::to_string(ice_parameters).unwrap();
        let ice_candidates = serde_json::to_string(ice_candidates).unwrap();
        let dtls_parameters = serde_json::to_string(dtls_parameters).unwrap();

        self.create_recv_transport_unchecked(
            transport_id,
            ice_parameters,
            ice_candidates,
            dtls_parameters,
        )
    }

    pub fn create_recv_transport_unchecked(
        self,
        transport_id: impl Into<Vec<u8>>,
        ice_parameters: impl Into<Vec<u8>>,
        ice_candidates: impl Into<Vec<u8>>,
        dtls_parameters: impl Into<Vec<u8>>,
    ) -> Device<Receiver> {
        let transport_id = CString::new(transport_id).unwrap();
        let ice_parameters = CString::new(ice_parameters).unwrap();
        let ice_candidates = CString::new(ice_candidates).unwrap();
        let dtls_parameters = CString::new(dtls_parameters).unwrap();

        let transport = unsafe {
            msc_create_recv_transport(
                self.device.0,
                transport_id.as_ptr(),
                ice_parameters.as_ptr(),
                ice_candidates.as_ptr(),
                dtls_parameters.as_ptr(),
            )
        };

        self.recv_transport.set(transport);

        Device {
            device: self.device,
            recv_transport: self.recv_transport,
            send_transport: self.send_transport,
            signaling: self.signaling,
            _state: PhantomData,
        }
    }
}

impl Device<Receiver> {
    pub fn create_audio_consumer<F>(
        &mut self,
        id: String,
        producer_id: String,
        rtp_parameters: &RtpParameters,
        on_audio_data: F,
    ) -> AudioSink
    where
        F: (FnMut(AudioData)) + Send + 'static,
    {
        let rtp_parameters = serde_json::to_string(rtp_parameters).unwrap();
        self.create_audio_consumer_unchecked(id, producer_id, rtp_parameters, on_audio_data)
    }

    pub fn create_audio_consumer_unchecked<F>(
        &mut self,
        id: impl Into<Vec<u8>>,
        producer_id: impl Into<Vec<u8>>,
        rtp_parameters: impl Into<Vec<u8>>,
        on_audio_data: F,
    ) -> AudioSink
    where
        F: (FnMut(AudioData)) + Send + 'static,
    {
        let id = CString::new(id).unwrap();
        let producer_id = CString::new(producer_id).unwrap();
        let rtp_parameters = CString::new(rtp_parameters).unwrap();

        let consumer = unsafe {
            msc_create_consumer(
                self.device.0,
                self.recv_transport.get(),
                id.as_ptr(),
                producer_id.as_ptr(),
                MediaKind::Audio as u32,
                rtp_parameters.as_ptr(),
            )
        };

        unsafe extern "C" fn on_audio_data_handler<F>(ctx: *mut c_void, audio_data: MscAudioData)
        where
            F: FnMut(AudioData) + 'static,
        {
            let data = unsafe {
                std::slice::from_raw_parts(
                    audio_data.data as *mut u8,
                    audio_data.number_of_frames as usize
                        * audio_data.number_of_channels as usize
                        * audio_data.sample_rate as usize
                        * audio_data.bits_per_sample as usize
                        / 8,
                )
            };

            (*(ctx as *mut F))(AudioData {
                data: data,
                bits_per_sample: audio_data.bits_per_sample as _,
                sample_rate: audio_data.sample_rate as _,
                number_of_channels: audio_data.number_of_channels as _,
                number_of_frames: audio_data.number_of_frames as _,
                timestamp_ms: audio_data.absolute_capture_timestamp_ms as _,
            });
        }

        let on_audio_data = Box::new(on_audio_data);

        let sink = unsafe {
            msc_add_audio_sink(
                consumer,
                on_audio_data.as_ref() as *const F as *mut c_void,
                Some(on_audio_data_handler::<F>),
            )
        };

        AudioSink {
            consumer,
            sink,
            _callback: on_audio_data,
        }
    }

    pub fn create_video_consumer<F>(
        &mut self,
        id: String,
        producer_id: String,
        rtp_parameters: &RtpParameters,
        on_video_frame: F,
    ) -> VideoSink
    where
        F: (FnMut(VideoFrame)) + Send + 'static,
    {
        let rtp_parameters = serde_json::to_string(rtp_parameters).unwrap();
        self.create_video_consumer_unchecked(id, producer_id, rtp_parameters, on_video_frame)
    }

    pub fn create_video_consumer_unchecked<F>(
        &mut self,
        id: impl Into<Vec<u8>>,
        producer_id: impl Into<Vec<u8>>,
        rtp_parameters: impl Into<Vec<u8>>,
        on_video_frame: F,
    ) -> VideoSink
    where
        F: (FnMut(VideoFrame)) + Send + 'static,
    {
        let id = CString::new(id).unwrap();
        let producer_id = CString::new(producer_id).unwrap();
        let rtp_parameters = CString::new(rtp_parameters).unwrap();

        let consumer = unsafe {
            msc_create_consumer(
                self.device.0,
                self.recv_transport.get(),
                id.as_ptr(),
                producer_id.as_ptr(),
                MediaKind::Video as u32,
                rtp_parameters.as_ptr(),
            )
        };

        unsafe extern "C" fn on_video_frame_handler<F>(ctx: *mut c_void, video_frame: MscVideoFrame)
        where
            F: FnMut(VideoFrame) + 'static,
        {
            let data = unsafe {
                std::slice::from_raw_parts(
                    video_frame.data as *mut u8,
                    video_frame.width as usize * video_frame.height as usize * 4,
                )
            };

            (*(ctx as *mut F))(VideoFrame {
                data: data,
                width: video_frame.width,
                height: video_frame.height,
                timestamp_ms: video_frame.timestamp,
            });
        }

        let on_video_frame = Box::new(on_video_frame);

        let sink = unsafe {
            msc_add_video_sink(
                consumer,
                on_video_frame.as_ref() as *const F as *mut c_void,
                Some(on_video_frame_handler::<F>),
            )
        };

        VideoSink {
            consumer,
            sink,
            _callback: on_video_frame,
        }
    }
}

pub struct AudioSink {
    consumer: *mut MscConsumer,
    sink: *mut MscConsumerSink,
    _callback: Box<dyn (FnMut(AudioData)) + Send + 'static>,
}

unsafe impl Send for AudioSink {}

impl Drop for AudioSink {
    fn drop(&mut self) {
        unsafe {
            msc_remove_audio_sink(self.consumer, self.sink);
            msc_free_consumer(self.consumer);
        }
    }
}

pub struct VideoSink {
    consumer: *mut MscConsumer,
    sink: *mut MscConsumerSink,
    _callback: Box<dyn (FnMut(VideoFrame)) + Send + 'static>,
}

unsafe impl Send for VideoSink {}

impl Drop for VideoSink {
    fn drop(&mut self) {
        unsafe {
            msc_remove_video_sink(self.consumer, self.sink);
            msc_free_consumer(self.consumer);
        }
    }
}
