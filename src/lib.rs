use std::{
    ffi::{CStr, CString},
    os::raw::{c_char, c_void},
};

pub use crate::types::*;

pub mod ffi;
mod types;

pub fn initialize() {
    unsafe { ffi::msc_initialize() };
}

pub fn cleanup() {
    unsafe { ffi::msc_cleanup() };
}

pub fn version() -> &'static str {
    use once_cell::sync::OnceCell;

    static VERSION: OnceCell<String> = OnceCell::new();

    VERSION.get_or_init(|| {
        let version = unsafe { CStr::from_ptr(ffi::msc_version()) };
        let out = version.to_string_lossy().to_string();
        unsafe { ffi::msc_free_string(version.as_ptr()) };
        out
    })
}

#[async_trait::async_trait]
pub trait Signaling {
    fn on_connect(&mut self, dtls_parameters: &DtlsParameters);

    fn on_connection_state_change(&mut self, connection_state: &str);

    async fn on_produce(&mut self, kind: MediaKind, rtp_parameters: &RtpParameters) -> ProducerId;
}

pub struct Device {
    inner: Box<Inner>,
}

struct Inner {
    device: *mut ffi::MscDevice,
    signaling: Box<dyn Signaling>,
}

impl Drop for Inner {
    fn drop(&mut self) {
        unsafe { ffi::msc_free_device(self.device) }
    }
}

impl Device {
    pub fn new<T: 'static + Signaling>(signaling: T) -> Self {
        let signaling = Box::new(signaling);
        Self::new_from_boxed(signaling)
    }

    pub fn new_from_boxed(signaling: Box<dyn Signaling>) -> Self {
        let device = unsafe { ffi::msc_alloc_device() };
        let mut inner = Box::new(Inner { device, signaling });

        unsafe extern "C" fn on_connect_handler(
            _transport: *mut ffi::MscTransport,
            user_ptr: *mut c_void,
            dtls_parameters: *const c_char,
        ) {
            let inner = &mut *(user_ptr as *mut Inner);
            let dtls_parameters: DtlsParameters =
                serde_json::from_slice(CStr::from_ptr(dtls_parameters).to_bytes()).unwrap();

            inner.signaling.on_connect(&dtls_parameters);
        }

        unsafe extern "C" fn on_connection_state_change_handler(
            _transport: *mut ffi::MscTransport,
            user_ptr: *mut c_void,
            connection_state: *const c_char,
        ) {
            let inner = &mut *(user_ptr as *mut Inner);
            let connection_state = CStr::from_ptr(connection_state);

            inner
                .signaling
                .on_connection_state_change(connection_state.to_str().unwrap());
        }

        unsafe extern "C" fn on_produce_handler(
            _transport: *mut ffi::MscTransport,
            user_ptr: *mut c_void,
            promise_id: i64,
            kind: ffi::MscMediaKind,
            rtp_parameters: *const c_char,
        ) {
            // tokio::spawn(async move {
            //     let inner = &mut *(user_ptr as *mut Inner);
            //     let rtp_parameters: RtpParameters =
            //         serde_json::from_slice(CStr::from_ptr(rtp_parameters).to_bytes()).unwrap();

            //     let producer_id = inner
            //         .signaling
            //         .on_produce(
            //             match kind {
            //                 ffi::MSC_MEDIA_KIND_AUDIO => MediaKind::Audio,
            //                 ffi::MSC_MEDIA_KIND_VIDEO => MediaKind::Video,
            //                 _ => unreachable!(),
            //             },
            //             &rtp_parameters,
            //         )
            //         .await;

            //     let producer_id = CString::new(producer_id.0).unwrap();
            //     ffi::msc_fulfill_producer_id(inner.device, promise_id, producer_id.as_ptr());
            // });
        }

        unsafe {
            ffi::msc_set_user_ptr(device, &mut *inner as *mut Inner as *mut c_void);
            ffi::msc_set_on_connect_handler(device, on_connect_handler);
            ffi::msc_set_on_connection_state_change_handler(
                device,
                on_connection_state_change_handler,
            );
            ffi::msc_set_on_produce_handler(device, on_produce_handler);
        }

        Self { inner }
    }

    pub fn load(&mut self, rtp_capabilities: &RtpCapabilities) -> bool {
        let rtp_capabilities = serde_json::to_vec(rtp_capabilities).unwrap();
        let rtp_capabilities = CString::new(rtp_capabilities).unwrap();
        unsafe { ffi::msc_load(self.inner.device, rtp_capabilities.as_ptr()) }
    }

    pub fn is_loaded(&self) -> bool {
        unsafe { ffi::msc_is_loaded(self.inner.device) }
    }

    pub fn can_produce(&self, kind: MediaKind) -> bool {
        unsafe { ffi::msc_can_produce(self.inner.device, kind as u32) }
    }
}
