use std::error::Error;

use rusty_msc::{
    initialize, AudioData, Device, DtlsParameters, IceCandidate, IceParameters, MediaKind,
    ProducerId, RtpCapabilities, RtpParameters, VideoFrame,
};

pub extern crate rusty_msc;

const ENDPOINT: &'static str = "https://portal-mediasoup-dev.service.zingplay.com";
const BEARER_AUTH: &'static str = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1aWQiOiI5IiwicmlkIjoiOSIsImlhdCI6MTY4NzI1MzQ5MiwiZXhwIjoxNzg3MjU3MDkyfQ.q099PYyLjLuGC7nWlOuEFPDyKEmgqSrIWwHEqsgLXyc";

struct Signal {
    client: reqwest::blocking::Client,
    streamer_id: String,
}

impl rusty_msc::Signaling for Signal {
    fn connect_transport(&mut self, _transport_id: &str, dtls_parameters: &DtlsParameters) {
        println!("connecting transport");

        #[derive(Debug, serde::Serialize)]
        #[serde(rename_all = "camelCase")]
        struct Body<'a> {
            dtls_parameters: &'a DtlsParameters,
        }

        self.client
            .post(format!(
                "{ENDPOINT}/live/{}/connectTransport",
                self.streamer_id
            ))
            .bearer_auth(BEARER_AUTH)
            .json(&Body { dtls_parameters })
            .send()
            .unwrap();
    }

    fn on_connection_state_change(&mut self, transport_id: &str, connection_state: &str) {
        println!("[{transport_id}] connection state changed: {connection_state}");
    }

    fn create_producer(
        &mut self,
        transport_id: &str,
        kind: MediaKind,
        rtp_parameters: &RtpParameters,
    ) -> ProducerId {
        todo!()
    }
}

fn main() -> Result<(), Box<dyn Error>> {
    let streamer_id = "1268521857";

    let client = reqwest::blocking::Client::new();
    let rtp_capabilities = client
        .get(format!("{ENDPOINT}/live/{streamer_id}"))
        .bearer_auth(BEARER_AUTH)
        .send()?;

    initialize();

    let mut device = Device::new(Signal {
        client: reqwest::blocking::Client::new(),
        streamer_id: streamer_id.to_owned(),
    });

    device.load(&rtp_capabilities.json::<RtpCapabilities>()?);

    #[derive(Debug, serde::Serialize)]
    #[serde(rename_all = "camelCase")]
    struct RtpCapBody {
        rtp_capabilities: RtpCapabilities,
    }

    println!("creating server side recv transport");
    let recv_transport_info = client
        .post(format!("{ENDPOINT}/live/{streamer_id}/createTransport"))
        .bearer_auth(BEARER_AUTH)
        .json(&RtpCapBody {
            rtp_capabilities: device.rtp_capabilities(),
        })
        .send()?;

    #[derive(Debug, serde::Deserialize)]
    #[serde(rename_all = "camelCase")]
    struct RecvTransportInfo {
        id: String,
        ice_parameters: IceParameters,
        ice_candidates: Vec<IceCandidate>,
        dtls_parameters: DtlsParameters,
    }

    let recv_transport_info = recv_transport_info.json::<RecvTransportInfo>()?;

    println!("creating client side recv transport");
    let mut device = device.create_recv_transport(
        recv_transport_info.id,
        &recv_transport_info.ice_parameters,
        &recv_transport_info.ice_candidates,
        &recv_transport_info.dtls_parameters,
    );

    println!("consuming server side");
    let consumers = client
        .post(format!("{ENDPOINT}/live/{streamer_id}/consume"))
        .bearer_auth(BEARER_AUTH)
        .json(&RtpCapBody {
            rtp_capabilities: device.rtp_capabilities(),
        })
        .send()?;

    #[derive(Debug, serde::Deserialize, serde::Serialize)]
    #[serde(rename_all = "camelCase")]
    struct ConsumerInfo {
        consumer_id: String,
        producer_id: String,
        kind: String,
        producer_type: String,
        rtp_parameters: RtpParameters,
    }

    let consumers: Vec<ConsumerInfo> = consumers.json()?;
    for consumer in consumers {
        let (media_kind, is_screen) = match consumer.producer_type.as_str() {
            "screen" => (MediaKind::Video, true),
            "video" => (MediaKind::Video, false),
            "audio" => (MediaKind::Audio, false),
            _ => unreachable!(),
        };

        if media_kind == MediaKind::Audio {
            println!("consuming audio");
            let sink = device.create_audio_consumer_unchecked(
                consumer.consumer_id,
                consumer.producer_id,
                serde_json::to_string(&consumer.rtp_parameters).unwrap(),
                |data: AudioData| {
                    let _ = data;
                },
            );

            std::mem::forget(sink);
        } else {
            println!("consuming video");
            let sink = device.create_video_consumer_unchecked(
                consumer.consumer_id,
                consumer.producer_id,
                serde_json::to_string(&consumer.rtp_parameters).unwrap(),
                |data: VideoFrame| {
                    let _ = data;
                },
            );

            std::mem::forget(sink);
        }
    }

    println!("Done, exiting..");
    std::thread::park();

    Ok(())
}
