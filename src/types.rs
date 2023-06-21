use crate::ffi;

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone)]
pub struct AudioData<'a> {
    pub data: &'a [u8],
    pub bits_per_sample: i32,
    pub sample_rate: i32,
    pub number_of_channels: i32,
    pub number_of_frames: i32,
    pub timestamp_ms: i64,
}

#[derive(Debug, Clone)]
pub struct VideoFrame<'a> {
    pub data: &'a [u8],
    pub width: i32,
    pub height: i32,
    pub timestamp_ms: i64,
}

pub struct ProducerId(pub String);

#[derive(Debug, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct RtpCapabilities {
    pub codecs: Vec<RtpCodecCapability>,
    pub header_extensions: Vec<RtpHeaderExtension>,
}

#[repr(u32)]
#[derive(Debug, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "lowercase")]
pub enum MediaKind {
    Audio = ffi::MscMediaKind_Audio,
    Video = ffi::MscMediaKind_Video,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct RtcpFeedback {
    pub r#type: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub parameter: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct RtpCodecCapability {
    pub kind: MediaKind,
    pub mime_type: String,
    pub preferred_payload_type: i32,
    pub clock_rate: i32,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub channels: Option<i32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub parameters: Option<serde_json::Map<String, serde_json::Value>>,
    pub rtcp_feedback: Vec<RtcpFeedback>,
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
    pub kind: MediaKind,
    pub uri: String,
    pub preferred_id: i32,
    pub preferred_encrypt: bool,
    pub direction: Direction,
}

#[derive(Debug, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum DtlsRole {
    Auto,
    Client,
    Server,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct DtlsFingerprint {
    pub algorithm: String,
    pub value: String,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct DtlsParameters {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub role: Option<DtlsRole>,
    pub fingerprints: Vec<DtlsFingerprint>,
}

#[derive(Debug, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct RtpCodecParameters {
    pub mime_type: String,
    pub payload_type: i32,
    pub clock_rate: i32,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub channels: Option<i32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub parameters: Option<serde_json::Map<String, serde_json::Value>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub rtcp_feedback: Option<Vec<RtcpFeedback>>,
}

#[derive(Debug, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct RtpHeaderExtensionParameters {
    pub uri: String,
    pub id: i32,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub encrypt: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub parameters: Option<serde_json::Map<String, serde_json::Value>>,
}

#[derive(Debug, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct SSrc {
    pub ssrc: u32,
}

#[derive(Debug, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct RtpEncodingParameters {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub ssrc: Option<u32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub rid: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub codec_payload_type: Option<u8>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub rtx: Option<SSrc>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub dtx: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub scalability_mode: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub scale_resolution_down_by: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub max_bitrate: Option<u32>,
}

#[derive(Debug, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct RtcpParameters {
    #[serde(skip_serializing_if = "Option::is_none")]
    cname: Option<String>,
    reduced_size: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    mux: Option<bool>,
}

#[derive(Debug, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct RtpParameters {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub mid: Option<String>,
    pub codecs: Vec<RtpCodecParameters>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub header_extensions: Option<Vec<RtpHeaderExtensionParameters>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub encodings: Option<Vec<RtpEncodingParameters>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub rtcp: Option<RtcpParameters>,
}

#[derive(Debug, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct IceParameters {
    pub username_fragment: String,
    pub password: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub ice_lite: Option<bool>,
}

#[derive(Debug, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum Protocol {
    Udp,
    Tcp,
}

#[derive(Debug, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct IceCandidate {
    foundation: String,
    priority: i32,
    ip: String,
    protocol: Protocol,
    port: u16,
    r#type: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    tcp_type: Option<String>,
}
