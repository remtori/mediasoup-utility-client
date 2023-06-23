use std::{
    cell::RefCell,
    collections::HashMap,
    ffi::{CStr, CString},
    hash::Hash,
    marker::PhantomData,
    os::raw::{c_char, c_void},
    ptr::NonNull,
    sync::{Arc, Once},
};

pub use crate::{error::Error, types::*};

use crate::error::map_ffi_nullptr;

use error::*;
use ffi::*;
use once_cell::sync::OnceCell;

mod error;
pub mod ffi;
mod types;

fn initialize() {
    unsafe extern "C" fn print_fn(str: *const c_char, length: i32) {
        let bytes = std::slice::from_raw_parts(str as *const u8, length as usize + 1);
        match CStr::from_bytes_with_nul(bytes) {
            Ok(str) => println!("[FFI] {}", str.to_string_lossy()),
            Err(err) => println!("[FFI] print_error: {err}"),
        }
    }

    ffi_try_warn!(unsafe { msc_initialize(Some(print_fn), Some(crate::error::push_error)) });
}

#[allow(unused)]
fn cleanup() {
    ffi_try_warn!(unsafe { msc_cleanup() });
}

pub fn version() -> &'static str {
    static VERSION: OnceCell<String> = OnceCell::new();

    VERSION.get_or_init(|| {
        let version = unsafe { CStr::from_ptr(msc_version()) };
        let out = version.to_string_lossy().to_string();
        unsafe { msc_free_string(version.as_ptr()) };

        ffi_try_warn!();
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

struct DeviceHandle(NonNull<MscDevice>);

impl Drop for DeviceHandle {
    fn drop(&mut self) {
        ffi_try_warn!(unsafe { msc_free_device(self.0.as_ptr()) })
    }
}

struct TransportHandle {
    address: NonNull<MscTransport>,
    _device: Arc<DeviceHandle>,
    _ctx: Box<dyn std::any::Any>,
}

impl Drop for TransportHandle {
    fn drop(&mut self) {
        ffi_try_warn!(unsafe { msc_free_transport(self.address.as_ptr()) })
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
    pub fn new<S: 'static + Signaling<T>>(signaling: S) -> Result<Device<None, T>, Error> {
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
            let transport_id = ffi_try_propagate!(CStr::from_ptr(msc_transport_get_id(transport)));
            let transport_ctx = ffi_try_propagate!(msc_transport_get_ctx(transport));

            let dtls_parameters: DtlsParameters =
                serde_json::from_slice(CStr::from_ptr(dtls_parameters).to_bytes())
                    .expect("DtlsParameters structure changed");

            let signaling = &mut *(ctx as *mut S);
            signaling.connect_transport(
                &*(transport_ctx as *const T),
                &transport_id.to_string_lossy(),
                &dtls_parameters,
            );
        }

        unsafe extern "C" fn on_connection_state_change_handler<T, S: Signaling<T>>(
            _device: *mut MscDevice,
            transport: *mut MscTransport,
            ctx: *mut c_void,
            connection_state: *const c_char,
        ) {
            let transport_id = ffi_try_propagate!(CStr::from_ptr(msc_transport_get_id(transport)));
            let transport_ctx = ffi_try_propagate!(msc_transport_get_ctx(transport));

            let connection_state = CStr::from_ptr(connection_state);

            let signaling = &mut *(ctx as *mut S);
            signaling.on_connection_state_change(
                &*(transport_ctx as *const T),
                &transport_id.to_string_lossy(),
                &connection_state.to_string_lossy(),
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
            let transport_id = ffi_try_propagate!(CStr::from_ptr(msc_transport_get_id(transport)));
            let transport_ctx = ffi_try_propagate!(msc_transport_get_ctx(transport));

            let rtp_parameters: RtpParameters =
                serde_json::from_slice(CStr::from_ptr(rtp_parameters).to_bytes())
                    .expect("RtpParameters structure changed");

            let signaling = &mut *(ctx as *mut S);
            let producer_id = signaling.create_producer(
                &*(transport_ctx as *const T),
                &transport_id.to_string_lossy(),
                #[allow(non_upper_case_globals)]
                match kind {
                    MscMediaKind_Audio => MediaKind::Audio,
                    MscMediaKind_Video => MediaKind::Video,
                    _ => unreachable!(),
                },
                &rtp_parameters,
            );

            let producer_id = CString::new(producer_id.0).unwrap();
            ffi_try_propagate!(msc_fulfill_producer_id(
                device,
                promise_id,
                producer_id.as_ptr()
            ));
        }

        let signaling = Box::new(signaling);
        let device = NonNull::new(unsafe {
            msc_alloc_device(
                signaling.as_ref() as *const S as *mut c_void,
                Some(on_connect_handler::<T, S>),
                Some(on_connection_state_change_handler::<T, S>),
                Some(on_produce_handler::<T, S>),
            )
        })
        .ok_or_else(map_ffi_nullptr)?;

        Ok(Device {
            device: Arc::new(DeviceHandle(device)),
            device_rtp_capabilities: OnceCell::new(),
            transports: Default::default(),
            signaling: RefCell::new(signaling),
            _state: PhantomData,
        })
    }

    pub fn load(self, rtp_capabilities: &RtpCapabilities) -> Result<Device<Loaded, T>, Error> {
        let rtp_capabilities = serde_json::to_vec(rtp_capabilities)?;
        self.load_unchecked(rtp_capabilities)
    }

    pub fn load_unchecked(
        self,
        rtp_capabilities: impl Into<Vec<u8>>,
    ) -> Result<Device<Loaded, T>, Error> {
        let rtp_capabilities = CString::new(rtp_capabilities.into())?;

        unsafe {
            msc_load(self.device.0.as_ptr(), rtp_capabilities.as_ptr());
            check_for_ffi_exception()?;
        }

        Ok(Device {
            device: self.device,
            device_rtp_capabilities: self.device_rtp_capabilities,
            transports: self.transports,
            signaling: self.signaling,
            _state: PhantomData,
        })
    }
}

impl<T: 'static + Hash + Eq + Clone> Device<Loaded, T> {
    pub fn can_produce(&self, kind: MediaKind) -> Result<bool, Error> {
        let ret = unsafe { msc_can_produce(self.device.0.as_ptr(), kind as u32) };
        check_for_ffi_exception().map(|_| ret)
    }

    pub fn rtp_capabilities(&self) -> &RtpCapabilities {
        self.device_rtp_capabilities.get_or_init(|| {
            let caps = ffi_try_warn!(unsafe {
                CStr::from_ptr(msc_get_rtp_capabilities(self.device.0.as_ptr()))
            });
            let ret =
                serde_json::from_slice(caps.to_bytes()).expect("RtpCapabilities structure changed");

            ffi_try_warn!(unsafe { msc_free_string(caps.as_ptr()) });

            ret
        })
    }

    pub fn ensure_transport(
        &mut self,
        transport_id: &T,
        transport_kind: TransportKind,
    ) -> Result<(), Error> {
        self.get_or_create_transport(transport_id, transport_kind)?;
        Ok(())
    }

    fn get_or_create_transport(
        &mut self,
        transport_id: &T,
        transport_kind: TransportKind,
    ) -> Result<NonNull<MscTransport>, Error> {
        // TODO: Do the hashing here to avoid unnecessary .clone()
        let transport_id = transport_id.clone();
        let map_key = (transport_id, transport_kind);
        if let Some(transport) = self.transports.get(&map_key) {
            return Ok(transport.address);
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

        let id = CString::new(id)?;
        let ice_parameters = CString::new(serde_json::to_string(&ice_parameters)?)?;
        let ice_candidates = CString::new(serde_json::to_string(&ice_candidates)?)?;
        let dtls_parameters = CString::new(serde_json::to_string(&dtls_parameters)?)?;

        let mut transport_ctx = Box::new(transport_id.clone());
        let transport: NonNull<MscTransport> = NonNull::new(unsafe {
            msc_create_transport(
                self.device.0.as_ptr(),
                transport_kind == TransportKind::Send,
                id.as_ptr(),
                ice_parameters.as_ptr(),
                ice_candidates.as_ptr(),
                dtls_parameters.as_ptr(),
                transport_ctx.as_mut() as *mut T as *mut c_void,
            )
        })
        .ok_or_else(map_ffi_nullptr)?;

        self.transports.insert(
            (transport_id, transport_kind),
            TransportHandle {
                address: transport,
                _device: self.device.clone(),
                _ctx: transport_ctx,
            },
        );

        Ok(transport)
    }

    pub fn create_audio_consumer<C: AudioConsumer>(
        &mut self,
        transport_id: &T,
        id: String,
        producer_id: String,
        rtp_parameters: &RtpParameters,
        on_audio_data: C,
    ) -> Result<AudioSink<C>, Error> {
        let rtp_parameters = serde_json::to_string(rtp_parameters)?;
        self.create_audio_consumer_unchecked(
            transport_id,
            id,
            producer_id,
            rtp_parameters,
            on_audio_data,
        )
    }

    pub fn create_audio_consumer_unchecked<C: AudioConsumer>(
        &mut self,
        transport_id: &T,
        id: impl Into<Vec<u8>>,
        producer_id: impl Into<Vec<u8>>,
        rtp_parameters: impl Into<Vec<u8>>,
        on_audio_data: C,
    ) -> Result<AudioSink<C>, Error> {
        unsafe extern "C" fn on_audio_data_handler<C: AudioConsumer>(
            ctx: *mut c_void,
            audio_data: MscAudioData,
        ) {
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

            (*(ctx as *mut C)).on_audio(AudioData {
                data: data,
                bits_per_sample: audio_data.bits_per_sample as _,
                sample_rate: audio_data.sample_rate as _,
                number_of_channels: audio_data.number_of_channels as _,
                number_of_frames: audio_data.number_of_frames as _,
                timestamp_ms: audio_data.absolute_capture_timestamp_ms as _,
            });
        }

        let on_audio_data = Box::new(on_audio_data);

        let id = CString::new(id)?;
        let producer_id = CString::new(producer_id)?;
        let rtp_parameters = CString::new(rtp_parameters)?;

        let transport = self.get_or_create_transport(transport_id, TransportKind::Recv)?;

        let sink = NonNull::new(unsafe {
            msc_create_audio_sink(
                self.device.0.as_ptr(),
                transport.as_ptr(),
                id.as_ptr(),
                producer_id.as_ptr(),
                rtp_parameters.as_ptr(),
                on_audio_data.as_ref() as *const C as *mut c_void,
                Some(on_audio_data_handler::<C>),
            )
        })
        .ok_or_else(map_ffi_nullptr)?;

        Ok(AudioSink {
            sink,
            consumer: Some(on_audio_data),
        })
    }

    pub fn create_video_consumer<C: VideoConsumer>(
        &mut self,
        transport_id: &T,
        id: String,
        producer_id: String,
        rtp_parameters: &RtpParameters,
        on_video_frame: C,
    ) -> Result<VideoSink<C>, Error> {
        let rtp_parameters = serde_json::to_string(rtp_parameters)?;
        self.create_video_consumer_unchecked(
            transport_id,
            id,
            producer_id,
            rtp_parameters,
            on_video_frame,
        )
    }

    pub fn create_video_consumer_unchecked<C: VideoConsumer>(
        &mut self,
        transport_id: &T,
        id: impl Into<Vec<u8>>,
        producer_id: impl Into<Vec<u8>>,
        rtp_parameters: impl Into<Vec<u8>>,
        on_video_frame: C,
    ) -> Result<VideoSink<C>, Error> {
        unsafe extern "C" fn on_video_frame_handler<C: VideoConsumer>(
            ctx: *mut c_void,
            video_frame: MscVideoFrame,
        ) {
            let data = unsafe {
                std::slice::from_raw_parts(
                    video_frame.data as *mut u8,
                    video_frame.width as usize * video_frame.height as usize * 4,
                )
            };

            (*(ctx as *mut C)).on_video(VideoFrame {
                data: data,
                width: video_frame.width,
                height: video_frame.height,
                timestamp_ms: video_frame.timestamp,
            });
        }

        let on_video_frame = Box::new(on_video_frame);

        let id = CString::new(id)?;
        let producer_id = CString::new(producer_id)?;
        let rtp_parameters = CString::new(rtp_parameters)?;

        let transport = self.get_or_create_transport(transport_id, TransportKind::Recv)?;

        let sink = NonNull::new(unsafe {
            msc_create_video_sink(
                self.device.0.as_ptr(),
                transport.as_ptr(),
                id.as_ptr(),
                producer_id.as_ptr(),
                rtp_parameters.as_ptr(),
                on_video_frame.as_ref() as *const C as *mut c_void,
                Some(on_video_frame_handler::<C>),
            )
        })
        .ok_or_else(map_ffi_nullptr)?;

        Ok(VideoSink {
            sink,
            consumer: Some(on_video_frame),
        })
    }

    pub fn create_video_writer<C: VideoWriter>(
        &mut self,
        transport_id: &T,
        id: String,
        producer_id: String,
        rtp_parameters: &RtpParameters,
        on_video_frame: C,
    ) -> Result<VideoWriterSink<C>, Error> {
        let rtp_parameters = serde_json::to_string(rtp_parameters)?;
        self.create_video_writer_unchecked(
            transport_id,
            id,
            producer_id,
            rtp_parameters,
            on_video_frame,
        )
    }

    pub fn create_video_writer_unchecked<C: VideoWriter>(
        &mut self,
        transport_id: &T,
        id: impl Into<Vec<u8>>,
        producer_id: impl Into<Vec<u8>>,
        rtp_parameters: impl Into<Vec<u8>>,
        on_write: C,
    ) -> Result<VideoWriterSink<C>, Error> {
        unsafe extern "C" fn on_video_frame_writer<C: VideoWriter>(
            ctx: *mut c_void,
            buffer: *mut u8,
            buffer_length: std::ffi::c_int,
        ) -> i32 {
            println!("atleast me?");

            let data = unsafe { std::slice::from_raw_parts(buffer, buffer_length as usize) };
            (*(ctx as *mut C)).on_write(data);
            0
        }

        let on_video_frame = Box::new(on_write);

        let id = CString::new(id)?;
        let producer_id = CString::new(producer_id)?;
        let rtp_parameters = CString::new(rtp_parameters)?;

        let transport = self.get_or_create_transport(transport_id, TransportKind::Recv)?;

        let sink = NonNull::new(unsafe {
            msc_create_video_writer(
                self.device.0.as_ptr(),
                transport.as_ptr(),
                id.as_ptr(),
                producer_id.as_ptr(),
                rtp_parameters.as_ptr(),
                on_video_frame.as_ref() as *const C as *mut c_void,
                Some(on_video_frame_writer::<C>),
            )
        })
        .ok_or_else(map_ffi_nullptr)?;

        Ok(VideoWriterSink {
            sink,
            consumer: Some(on_video_frame),
        })
    }
}

pub trait AudioConsumer: 'static + Send {
    fn on_audio(&mut self, audio_data: AudioData);
}

pub trait VideoConsumer: 'static + Send {
    fn on_video(&mut self, video_frame: VideoFrame);
}

pub trait VideoWriter: 'static + Send {
    fn on_write(&mut self, data: &[u8]);
}

impl<F: (FnMut(AudioData)) + Send + 'static> AudioConsumer for F {
    fn on_audio(&mut self, audio_data: AudioData) {
        (self)(audio_data)
    }
}

impl<F: (FnMut(VideoFrame)) + Send + 'static> VideoConsumer for F {
    fn on_video(&mut self, video_frame: VideoFrame) {
        (self)(video_frame)
    }
}

impl<F: (FnMut(&[u8])) + Send + 'static> VideoWriter for F {
    fn on_write(&mut self, data: &[u8]) {
        (self)(data)
    }
}

pub struct AudioSink<C> {
    sink: NonNull<MscConsumerSink>,
    consumer: Option<Box<C>>,
}

unsafe impl<C> Send for AudioSink<C> {}

impl<C: AudioConsumer> AudioSink<C> {
    pub fn take(mut self) -> Box<C> {
        self.consumer.take().unwrap()
    }
}

impl<C> Drop for AudioSink<C> {
    fn drop(&mut self) {
        ffi_try_warn!(unsafe { msc_free_sink(self.sink.as_ptr()) })
    }
}

pub struct VideoSink<C> {
    sink: NonNull<MscConsumerSink>,
    consumer: Option<Box<C>>,
}

unsafe impl<C> Send for VideoSink<C> {}

impl<C: VideoConsumer> VideoSink<C> {
    pub fn take(mut self) -> Box<C> {
        self.consumer.take().unwrap()
    }
}

impl<C> Drop for VideoSink<C> {
    fn drop(&mut self) {
        ffi_try_warn!(unsafe { msc_free_sink(self.sink.as_ptr()) })
    }
}

pub struct VideoWriterSink<C> {
    sink: NonNull<MscConsumerSink>,
    consumer: Option<Box<C>>,
}

unsafe impl<C> Send for VideoWriterSink<C> {}

impl<C: VideoWriter> VideoWriterSink<C> {
    pub fn take(mut self) -> Box<C> {
        self.consumer.take().unwrap()
    }
}

impl<C> Drop for VideoWriterSink<C> {
    fn drop(&mut self) {
        ffi_try_warn!(unsafe { msc_free_sink(self.sink.as_ptr()) })
    }
}
