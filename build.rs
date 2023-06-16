use std::env;
use std::path::PathBuf;

fn main() {
    println!("cargo:rustc-link-search=deps/webrtc");
    println!("cargo:rustc-link-lib=webrtc");
    println!("cargo:rustc-link-lib=mediasoupclient-native");
    println!("cargo:rerun-if-changed=./src/lib.h");
    println!("cargo:rerun-if-changed=./src/lib.cc");

    // Don't build if "cargo doc" is being run. This is to support docs.rs.
    let is_cargo_doc = env::var_os("DOCS_RS").is_some();

    // Don't build if the rust language server (RLS) is running.
    let is_rls = env::var_os("CARGO")
        .map(PathBuf::from)
        .as_ref()
        .and_then(|p| p.file_stem())
        .and_then(|f| f.to_str())
        .map(|s| s.starts_with("rls"))
        .unwrap_or(false);

    // Early exit
    if is_cargo_doc || is_rls {
        build_bindgen();
        return;
    }

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

    build_bindgen();
}

fn build_bindgen() {
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
