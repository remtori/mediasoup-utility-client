cmake_minimum_required(VERSION 3.16)

project(msc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LIBWEBRTC_INCLUDE_PATH "${PROJECT_SOURCE_DIR}/deps/webrtc/include")
set(LIBWEBRTC_BINARY_PATH "${PROJECT_SOURCE_DIR}/deps/webrtc")

add_compile_options(
	-fPIE
	-Wno-deprecated-declarations
)

add_library(${PROJECT_NAME} ${SOURCES})

target_sources(${PROJECT_NAME} PRIVATE
    msc/lib.h
    msc/lib.cpp
    msc/device.hpp
    msc/device.cpp
    msc/media_sink.hpp
    msc/media_sink.cpp
    msc/peer_connection_factory.cpp
    msc/video_writer.hpp
    msc/video_writer.cpp
)

add_subdirectory(deps/fmt)
add_subdirectory(deps/libmediasoupclient)

target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
    deps/fmt/include
    deps/libmediasoupclient/include
    deps/libmediasoupclient/deps/libsdptransform/include
    deps/webrtc/include
    deps/webrtc/include/third_party/abseil-cpp
    deps/webrtc/include/third_party/libyuv/include
    deps/webrtc/include/third_party/ffmpeg
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    fmt::fmt
    mediasoupclient
)

target_compile_definitions(${PROJECT_NAME} PUBLIC
	$<$<NOT:$<PLATFORM_ID:Windows>>:WEBRTC_POSIX>
	$<$<PLATFORM_ID:Windows>:WEBRTC_WIN>
	$<$<PLATFORM_ID:Windows>:NOMINMAX>
	$<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
	$<$<PLATFORM_ID:Darwin>:WEBRTC_MAC>
    NDEBUG
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra -Wpedantic
)
