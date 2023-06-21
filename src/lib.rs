use std::{
    cell::RefCell,
    collections::HashMap,
    ffi::{CStr, CString},
    hash::Hash,
    marker::PhantomData,
    os::raw::{c_char, c_void},
    sync::{Arc, Once},
};

pub use crate::types::*;
use ffi::*;
use once_cell::sync::OnceCell;

pub mod ffi;
mod types;

fn initialize() {
    unsafe extern "C" fn print_fn(str: *const c_char, length: i32) {
        let bytes = std::slice::from_raw_parts(str as *const u8, length as usize + 1);
        let str = CStr::from_bytes_with_nul(bytes).unwrap();
        println!("[MSC FFI] {}", str.to_string_lossy());
    }

    unsafe { msc_initialize(Some(print_fn)) };
}

#[allow(unused)]
fn cleanup() {
    unsafe { msc_cleanup() };
}

pub fn version() -> &'static str {
    static VERSION: OnceCell<String> = OnceCell::new();

    VERSION.get_or_init(|| {
        let version = unsafe { CStr::from_ptr(msc_version()) };
        let out = version.to_string_lossy().to_string();
        unsafe { msc_free_string(version.as_ptr()) };
        out
    })
}

#[derive(Debug, serde::Serialize, serde::Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct TransportCreationOptions {
    pub id: String,
    pub ice_parameters: IceParameters,
    pub ice_candidates: Vec<IceCandidate>,
    pub dtls_parameters: DtlsParameters,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum TransportKind {
    Send,
    Recv,
}

pub trait Signaling<T>: Send + Sync {
    fn create_transport(
        &mut self,
        channel_id: &T,
        kind: TransportKind,
        rtp_capabilities: &RtpCapabilities,
    ) -> TransportCreationOptions;

    fn connect_transport(
        &mut self,
        channel_id: &T,
        transport_id: &str,
        dtls_parameters: &DtlsParameters,
    );

    fn create_producer(
        &mut self,
        channel_id: &T,
        transport_id: &str,
        kind: MediaKind,
        rtp_parameters: &RtpParameters,
    ) -> ProducerId;

    fn on_connection_state_change(
        &mut self,
        channel_id: &T,
        transport_id: &str,
        connection_state: &str,
    );
}

pub struct None;
pub struct Loaded;

struct DeviceHandle(*mut MscDevice);

impl Drop for DeviceHandle {
    fn drop(&mut self) {
        unsafe { msc_free_device(self.0) }
    }
}

struct TransportHandle {
    address: *mut MscTransport,
    _ctx: Box<dyn std::any::Any>,
}

impl Drop for TransportHandle {
    fn drop(&mut self) {
        unsafe { msc_free_transport(self.address) }
    }
}

pub struct Device<State, T: Hash + Eq> {
    device: Arc<DeviceHandle>,
    device_rtp_capabilities: OnceCell<RtpCapabilities>,
    transports: HashMap<(T, TransportKind), TransportHandle>,
    signaling: RefCell<Box<dyn Signaling<T>>>,
    _state: PhantomData<State>,
}

impl<S, T: Hash + Eq> std::fmt::Debug for Device<S, T> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Device")
            .field("device", &self.device.0)
            .field("rtp_capabilities", &self.device_rtp_capabilities.get())
            .finish()
    }
}

// unsafe impl Send for Inner {}
// unsafe impl Sync for Inner {}

impl<T: Hash + Eq> Device<None, T> {
    pub fn new<S: 'static + Signaling<T>>(signaling: S) -> Device<None, T> {
        static ONCE_INIT: Once = Once::new();
        ONCE_INIT.call_once(|| {
            initialize();
        });

        unsafe extern "C" fn on_connect_handler<T, S: Signaling<T>>(
            _device: *mut MscDevice,
            transport: *mut MscTransport,
            ctx: *mut c_void,
            dtls_parameters: *const c_char,
        ) {
            let transport_id = CStr::from_ptr(msc_transport_get_id(transport));
            let transport_ctx = msc_transport_get_ctx(transport);

            let dtls_parameters: DtlsParameters =
                serde_json::from_slice(CStr::from_ptr(dtls_parameters).to_bytes()).unwrap();

            let signaling = &mut *(ctx as *mut S);
            signaling.connect_transport(
                &*(transport_ctx as *const T),
                transport_id.to_str().unwrap(),
                &dtls_parameters,
            );
        }

        unsafe extern "C" fn on_connection_state_change_handler<T, S: Signaling<T>>(
            _device: *mut MscDevice,
            transport: *mut MscTransport,
            ctx: *mut c_void,
            connection_state: *const c_char,
        ) {
            let transport_id = CStr::from_ptr(msc_transport_get_id(transport));
            let transport_ctx = msc_transport_get_ctx(transport);

            let connection_state = CStr::from_ptr(connection_state);

            let signaling = &mut *(ctx as *mut S);
            signaling.on_connection_state_change(
                &*(transport_ctx as *const T),
                transport_id.to_str().unwrap(),
                connection_state.to_str().unwrap(),
            );
        }

        unsafe extern "C" fn on_produce_handler<T, S: Signaling<T>>(
            device: *mut MscDevice,
            transport: *mut MscTransport,
            ctx: *mut c_void,
            promise_id: i64,
            kind: MscMediaKind,
            rtp_parameters: *const c_char,
        ) {
            let transport_id = CStr::from_ptr(msc_transport_get_id(transport));
            let transport_ctx = msc_transport_get_ctx(transport);

            let rtp_parameters: RtpParameters =
                serde_json::from_slice(CStr::from_ptr(rtp_parameters).to_bytes()).unwrap();

            let signaling = &mut *(ctx as *mut S);
            let producer_id = signaling.create_producer(
                &*(transport_ctx as *const T),
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
                signaling.as_ref() as *const S as *mut c_void,
                Some(on_connect_handler::<T, S>),
                Some(on_connection_state_change_handler::<T, S>),
                Some(on_produce_handler::<T, S>),
            )
        };

        Device {
            device: Arc::new(DeviceHandle(device)),
            device_rtp_capabilities: OnceCell::new(),
            transports: Default::default(),
            signaling: RefCell::new(signaling),
            _state: PhantomData,
        }
    }

    pub fn load(self, rtp_capabilities: &RtpCapabilities) -> Device<Loaded, T> {
        let rtp_capabilities = serde_json::to_vec(rtp_capabilities).unwrap();
        self.load_unchecked(rtp_capabilities)
    }

    pub fn load_unchecked(self, rtp_capabilities: impl Into<Vec<u8>>) -> Device<Loaded, T> {
        let rtp_capabilities = CString::new(rtp_capabilities.into()).unwrap();
        unsafe { msc_load(self.device.0, rtp_capabilities.as_ptr()) };

        Device {
            device: self.device,
            device_rtp_capabilities: self.device_rtp_capabilities,
            transports: self.transports,
            signaling: self.signaling,
            _state: PhantomData,
        }
    }
}

impl<T: 'static + Hash + Eq + Clone> Device<Loaded, T> {
    pub fn can_produce(&self, kind: MediaKind) -> bool {
        unsafe { msc_can_produce(self.device.0, kind as u32) }
    }

    pub fn rtp_capabilities(&self) -> &RtpCapabilities {
        self.device_rtp_capabilities.get_or_init(|| {
            let caps = unsafe { CStr::from_ptr(msc_get_rtp_capabilities(self.device.0)) };
            let ret = serde_json::from_slice(caps.to_bytes()).unwrap();
            unsafe { msc_free_string(caps.as_ptr()) };
            ret
        })
    }

    fn get_or_create_transport(
        &mut self,
        transport_id: &T,
        transport_kind: TransportKind,
    ) -> *mut MscTransport {
        // TODO: Do the hashing here to avoid unnecessary .clone()
        let transport_id = transport_id.clone();
        let map_key = (transport_id, transport_kind);
        if let Some(transport) = self.transports.get(&map_key) {
            return transport.address;
        }

        let transport_id = map_key.0;
        let TransportCreationOptions {
            id,
            ice_parameters,
            ice_candidates,
            dtls_parameters,
        } = self.signaling.borrow_mut().create_transport(
            &transport_id,
            TransportKind::Send,
            self.rtp_capabilities(),
        );

        let id = CString::new(id).unwrap();
        let ice_parameters = CString::new(serde_json::to_string(&ice_parameters).unwrap()).unwrap();
        let ice_candidates = CString::new(serde_json::to_string(&ice_candidates).unwrap()).unwrap();
        let dtls_parameters =
            CString::new(serde_json::to_string(&dtls_parameters).unwrap()).unwrap();

        let mut transport_ctx = Box::new(transport_id.clone());
        let transport = unsafe {
            msc_create_transport(
                self.device.0,
                transport_kind == TransportKind::Send,
                id.as_ptr(),
                ice_parameters.as_ptr(),
                ice_candidates.as_ptr(),
                dtls_parameters.as_ptr(),
                transport_ctx.as_mut() as *mut T as *mut c_void,
            )
        };

        self.transports.insert(
            (transport_id, transport_kind),
            TransportHandle {
                address: transport,
                _ctx: transport_ctx,
            },
        );

        transport
    }

    pub fn create_audio_consumer<F>(
        &mut self,
        transport_id: &T,
        id: String,
        producer_id: String,
        rtp_parameters: &RtpParameters,
        on_audio_data: F,
    ) -> AudioSink
    where
        F: (FnMut(AudioData)) + Send + 'static,
    {
        let rtp_parameters = serde_json::to_string(rtp_parameters).unwrap();
        self.create_audio_consumer_unchecked(
            transport_id,
            id,
            producer_id,
            rtp_parameters,
            on_audio_data,
        )
    }

    pub fn create_audio_consumer_unchecked<F>(
        &mut self,
        transport_id: &T,
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

        let transport = self.get_or_create_transport(transport_id, TransportKind::Recv);
        let consumer = unsafe {
            msc_create_consumer(
                self.device.0,
                transport,
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
        transport_id: &T,
        id: String,
        producer_id: String,
        rtp_parameters: &RtpParameters,
        on_video_frame: F,
    ) -> VideoSink
    where
        F: (FnMut(VideoFrame)) + Send + 'static,
    {
        let rtp_parameters = serde_json::to_string(rtp_parameters).unwrap();
        self.create_video_consumer_unchecked(
            transport_id,
            id,
            producer_id,
            rtp_parameters,
            on_video_frame,
        )
    }

    pub fn create_video_consumer_unchecked<F>(
        &mut self,
        transport_id: &T,
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

        let transport = self.get_or_create_transport(transport_id, TransportKind::Recv);
        let consumer = unsafe {
            msc_create_consumer(
                self.device.0,
                transport,
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
