cmake_minimum_required(VERSION 3.16)

add_compile_options(
	-fPIE -fPIC
	-Wno-deprecated-declarations
)

set(LIBWEBRTC_INCLUDE_PATH "${PROJECT_SOURCE_DIR}/deps/webrtc/include")
set(LIBWEBRTC_BINARY_PATH "${PROJECT_SOURCE_DIR}/deps/webrtc/prebuilt/x86_64-unknown-linux")

add_subdirectory(asio)
add_subdirectory(fmt)
add_subdirectory(libmediasoupclient)
