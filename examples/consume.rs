use std::error::Error;

use rusty_msc::{
    AudioData, Device, DtlsParameters, MediaKind, ProducerId, RtpCapabilities, RtpParameters,
    VideoFrame,
};

pub extern crate rusty_msc;

const ENDPOINT: &'static str = "https://portal-mediasoup-dev.service.zingplay.com";
const BEARER_AUTH: &'static str = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1aWQiOiI5IiwicmlkIjoiOSIsImlhdCI6MTY4NzI1MzQ5MiwiZXhwIjoxNzg3MjU3MDkyfQ.q099PYyLjLuGC7nWlOuEFPDyKEmgqSrIWwHEqsgLXyc";

#[derive(Debug, serde::Serialize)]
#[serde(rename_all = "camelCase")]
struct RtpCapBody<'a> {
    rtp_capabilities: &'a RtpCapabilities,
}

struct Signal {
    client: reqwest::blocking::Client,
}

impl rusty_msc::Signaling<String> for Signal {
    fn create_transport(
        &mut self,
        channel_id: &String,
        _kind: rusty_msc::TransportKind,
        rtp_capabilities: &RtpCapabilities,
    ) -> rusty_msc::TransportCreationOptions {
        println!("creating server side transport");

        let recv_transport_info = self
            .client
            .post(format!("{ENDPOINT}/live/{}/createTransport", channel_id))
            .bearer_auth(BEARER_AUTH)
            .json(&RtpCapBody { rtp_capabilities })
            .send()
            .unwrap();

        recv_transport_info.json().unwrap()
    }

    fn connect_transport(
        &mut self,
        channel_id: &String,
        _transport_id: &str,
        dtls_parameters: &DtlsParameters,
    ) {
        println!("connecting transport");

        #[derive(Debug, serde::Serialize)]
        #[serde(rename_all = "camelCase")]
        struct Body<'a> {
            dtls_parameters: &'a DtlsParameters,
        }

        self.client
            .post(format!("{ENDPOINT}/live/{}/connectTransport", channel_id))
            .bearer_auth(BEARER_AUTH)
            .json(&Body { dtls_parameters })
            .send()
            .unwrap();
    }

    fn on_connection_state_change(
        &mut self,
        channel_id: &String,
        transport_id: &str,
        connection_state: &str,
    ) {
        println!("[{channel_id}][{transport_id}] connection state changed: {connection_state}");
    }

    fn create_producer(
        &mut self,
        _channel_id: &String,
        _transport_id: &str,
        _kind: MediaKind,
        _rtp_parameters: &RtpParameters,
    ) -> ProducerId {
        todo!()
    }
}

fn main() -> Result<(), Box<dyn Error>> {
    let mut last = None;
    let streamer_id = "1268521857".to_owned();

    let client = reqwest::blocking::Client::new();
    let rtp_capabilities = client
        .get(format!("{ENDPOINT}/live/{streamer_id}"))
        .bearer_auth(BEARER_AUTH)
        .send()?;

    let device = Device::new(Signal {
        client: client.clone(),
    })?;

    let mut device = device.load(&rtp_capabilities.json::<RtpCapabilities>()?)?;

    device.ensure_transport(&streamer_id, rusty_msc::TransportKind::Recv)?;

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
        let (media_kind, _is_screen) = match consumer.producer_type.as_str() {
            "screen" => (MediaKind::Video, true),
            "video" => (MediaKind::Video, false),
            "audio" => (MediaKind::Audio, false),
            _ => unreachable!(),
        };

        if media_kind == MediaKind::Audio {
            println!("consuming audio");
            let sink = device.create_audio_consumer(
                &streamer_id,
                consumer.consumer_id,
                consumer.producer_id,
                &consumer.rtp_parameters,
                |data: AudioData| {
                    let _ = data;
                },
            )?;

            std::mem::forget(sink);
        } else {
            println!("consuming video");
            let sink = device.create_video_consumer(
                &streamer_id,
                consumer.consumer_id,
                consumer.producer_id,
                &consumer.rtp_parameters,
                |data: VideoFrame| {
                    let _ = data;
                    println!(
                        "got video {}x{} at {}",
                        data.width, data.height, data.timestamp_ms
                    );
                },
            )?;

            // std::mem::forget(sink);
            last = Some(sink);
        }
    }

    client
        .post(format!("{ENDPOINT}/live/{streamer_id}/resume"))
        .bearer_auth(BEARER_AUTH)
        .json(&serde_json::Map::new())
        .send()?;

    println!("Done, exiting..");
    // std::thread::park();

    Ok(())
}
