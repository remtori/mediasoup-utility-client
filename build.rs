use std::env;
use std::path::PathBuf;

fn main() {
    println!("cargo:rustc-link-search=deps/webrtc");
    println!("cargo:rustc-link-lib=webrtc");
    println!("cargo:rustc-link-lib=mediasoupclient-native");
    println!("cargo:rerun-if-changed=./src/lib.h");

    cc::Build::new()
        .cpp(true)
        .flag("-std=c++20")
        .flag("-DWEBRTC_POSIX -fexceptions")
        .static_flag(true)
        .includes(&[
            "deps/webrtc/include",
            "deps/webrtc/include/third_party/abseil-cpp",
            "deps/webrtc/include/third_party/libyuv/include",
            "deps/libmediasoupclient/include",
            "deps/libmediasoupclient/deps/libsdptransform/include",
        ])
        .files(&[
            "deps/libmediasoupclient/src/Consumer.cpp",
            "deps/libmediasoupclient/src/DataConsumer.cpp",
            "deps/libmediasoupclient/src/DataProducer.cpp",
            "deps/libmediasoupclient/src/Device.cpp",
            "deps/libmediasoupclient/src/Handler.cpp",
            "deps/libmediasoupclient/src/Logger.cpp",
            "deps/libmediasoupclient/src/PeerConnection.cpp",
            "deps/libmediasoupclient/src/Producer.cpp",
            "deps/libmediasoupclient/src/Transport.cpp",
            "deps/libmediasoupclient/src/mediasoupclient.cpp",
            "deps/libmediasoupclient/src/ortc.cpp",
            "deps/libmediasoupclient/src/scalabilityMode.cpp",
            "deps/libmediasoupclient/src/sdp/MediaSection.cpp",
            "deps/libmediasoupclient/src/sdp/RemoteSdp.cpp",
            "deps/libmediasoupclient/src/sdp/Utils.cpp",
            "deps/libmediasoupclient/deps/libsdptransform/src/grammar.cpp",
            "deps/libmediasoupclient/deps/libsdptransform/src/parser.cpp",
            "deps/libmediasoupclient/deps/libsdptransform/src/writer.cpp",
            "src/lib.cpp",
        ])
        .compile("mediasoupclient-native");

    let bindings = bindgen::Builder::default()
        .header("src/lib.h")
        .clang_arg("-DGEN_BINDINGS")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks))
        .generate()
        .expect("Unable to generate bindings");

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}
