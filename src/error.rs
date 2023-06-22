use std::sync::Mutex;

use once_cell::sync::Lazy;
use thiserror::Error;

#[derive(Debug, Error)]
pub enum Error {
    #[error("ffi function return nullptr for unknown reason")]
    FfiReturnNullptr,
    #[error("{0}")]
    FfiException(FfiExceptions),
    #[error("string contains null byte {0}")]
    StringNullByte(#[from] std::ffi::NulError),
    #[error("serde error {0}")]
    Serde(#[from] serde_json::Error),
}

#[derive(Debug, Error)]
pub enum FfiException {
    #[error("ffi unknown exception")]
    Unknown,
    #[error("ffi json parse error: {0}")]
    JsonParseError(String),
    #[error("ffi json error: {0}")]
    JsonInvalidIter(String),
    #[error("ffi json type error: {0}")]
    JsonTypeError(String),
    #[error("ffi json out of range error: {0}")]
    JsonOutOfRange(String),
    #[error("ffi json error: {0}")]
    JsonOther(String),
    #[error("ffi mediasoup type error: {0}")]
    MediasoupTypeError(String),
    #[error("ffi mediasoup unsupported error: {0}")]
    MediasoupUnsupportedError(String),
    #[error("ffi mediasoup invalid state error: {0}")]
    MediasoupInvalidStateError(String),
    #[error("ffi mediasoup error: {0}")]
    MediasoupOther(String),
    #[error("ffi exception: {0}")]
    Exception(String),
}

#[derive(Debug)]
pub struct FfiExceptions {
    exceptions: Vec<FfiException>,
}

impl std::fmt::Display for FfiExceptions {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("[")?;
        for ex in &self.exceptions {
            if f.alternate() {
                f.write_str("\n\t")?;
            }

            write!(f, "{}", ex)?;
            f.write_str(",")?;
        }
        f.write_str("]")?;

        Ok(())
    }
}

#[macro_export]
macro_rules! ffi_try_propagate {
    ($e:expr) => {{
        let __r = $e;
        if $crate::error::has_ffi_exception() {
            return;
        }

        __r
    }};
}

#[macro_export]
macro_rules! ffi_try_warn {
    () => {
        $crate::error::warn_ffi_exception(file!(), line!());
    };
    ($e:expr) => {{
        let __r = $e;
        $crate::error::warn_ffi_exception(file!(), line!());
        __r
    }};
}

static ERROR_STACK: Lazy<Mutex<Vec<FfiException>>> = Lazy::new(|| Mutex::new(Vec::new()));

pub(crate) fn has_ffi_exception() -> bool {
    if let Ok(error_stack) = ERROR_STACK.lock() {
        !error_stack.is_empty()
    } else {
        false
    }
}

pub(crate) fn map_ffi_nullptr() -> Error {
    check_for_ffi_exception()
        .err()
        .unwrap_or(Error::FfiReturnNullptr)
}

pub(crate) fn warn_ffi_exception(file: &'static str, line: u32) {
    if let Err(err) = check_for_ffi_exception() {
        println!("{file}:{line} {err}");
    }
}

pub(crate) fn check_for_ffi_exception() -> Result<(), Error> {
    if let Ok(mut error_stack) = ERROR_STACK.lock() {
        if !error_stack.is_empty() {
            return Err(Error::FfiException(FfiExceptions {
                exceptions: error_stack.drain(..).collect::<Vec<_>>(),
            }));
        }
    }

    Ok(())
}

pub(crate) unsafe extern "C" fn push_error(
    kind: crate::ffi::MscErrorKind,
    message: *const std::ffi::c_char,
) {
    use crate::ffi::*;

    let message = if message.is_null() {
        "<empty>".to_string()
    } else {
        std::ffi::CStr::from_ptr(message)
            .to_string_lossy()
            .to_string()
    };

    #[allow(non_upper_case_globals)]
    let err = match kind {
        MscErrorKind_Unknown => FfiException::Unknown,
        MscErrorKind_JsonParseError => FfiException::JsonParseError(message),
        MscErrorKind_JsonInvalidIter => FfiException::JsonInvalidIter(message),
        MscErrorKind_JsonTypeError => FfiException::JsonTypeError(message),
        MscErrorKind_JsonOutOfRange => FfiException::JsonOutOfRange(message),
        MscErrorKind_JsonOther => FfiException::JsonOther(message),
        MscErrorKind_MediasoupTypeError => FfiException::MediasoupTypeError(message),
        MscErrorKind_MediasoupUnsupportedError => FfiException::MediasoupUnsupportedError(message),
        MscErrorKind_MediasoupInvalidStateError => {
            FfiException::MediasoupInvalidStateError(message)
        }
        MscErrorKind_MediasoupOther => FfiException::MediasoupOther(message),
        MscErrorKind_Exception => FfiException::Exception(message),
        _ => unreachable!(),
    };

    if let Ok(mut error_stack) = ERROR_STACK.lock() {
        error_stack.push(err);
    }
}
