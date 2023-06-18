use crate::ffi;

use serde::{Deserialize, Serialize};

pub struct ProducerId(pub String);

#[derive(Debug, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct RtpCapabilities {
    pub codecs: Vec<RtpCodecCapability>,
    pub header_extensions: Vec<RtpHeaderExtension>,
}

#[derive(Debug, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum MediaKind {
    Audio = ffi::MSC_MEDIA_KIND_AUDIO as isize,
    Video = ffi::MSC_MEDIA_KIND_VIDEO as isize,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct RtcpFeedback {}

#[derive(Debug, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct RtpCodecCapability {
    kind: MediaKind,
    mime_type: String,
    preferred_payload_type: i32,
    clock_rate: i32,
    channels: Option<i32>,
    parameters: Option<serde_json::Map<String, serde_json::Value>>,
    rtcp_feedback: Vec<RtcpFeedback>,
}

#[derive(Debug, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum Direction {
    SendRecv,
    SendOnly,
    RecvOnly,
}

#[derive(Debug, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct RtpHeaderExtension {
    kind: MediaKind,
    uri: String,
    preferred_id: i32,
    preferred_encrypt: bool,
    direction: Direction,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct DtlsParameters {}

#[derive(Debug, Serialize, Deserialize)]
pub struct RtpParameters {}
