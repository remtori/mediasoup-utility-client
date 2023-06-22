use thiserror::Error;

#[derive(Debug, Error)]
pub enum Error {
    #[error("ffi function return nullptr for unknown reason")]
    FfiReturnNullptr,
    #[error("string contains null byte {0}")]
    StringNullByte(#[from] std::ffi::NulError),
    #[error("serde error {0}")]
    Serde(#[from] serde_json::Error),
}
