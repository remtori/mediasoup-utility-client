cmake_minimum_required(VERSION 3.16)

project(msc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_sources(${PROJECT_NAME} PRIVATE
	include/msc/executor.hpp
	include/msc/json.hpp
	include/msc/msc.hpp
	src/assert.hpp
    src/media_sink.hpp
    src/media_sink.cpp
	src/msc.cpp
    src/peer_connection_factory.cpp
)

target_include_directories(${PROJECT_NAME} SYSTEM
PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
PRIVATE
    ${CMAKE_SOURCE_DIR}/deps/fmt/include
    ${CMAKE_SOURCE_DIR}/deps/libmediasoupclient/include
    ${CMAKE_SOURCE_DIR}/deps/libmediasoupclient/deps/libsdptransform/include
    ${CMAKE_SOURCE_DIR}/deps/webrtc/include
    ${CMAKE_SOURCE_DIR}/deps/webrtc/include/third_party/abseil-cpp
    ${CMAKE_SOURCE_DIR}/deps/webrtc/include/third_party/libyuv/include
    ${CMAKE_SOURCE_DIR}/deps/webrtc/include/third_party/ffmpeg
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    fmt::fmt
    mediasoupclient
	$<$<NOT:$<PLATFORM_ID:Windows>>:pthread>
	$<$<NOT:$<PLATFORM_ID:Windows>>:X11>
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
	$<$<NOT:$<PLATFORM_ID:Windows>>:WEBRTC_POSIX>
	$<$<PLATFORM_ID:Windows>:WEBRTC_WIN>
	$<$<PLATFORM_ID:Windows>:NOMINMAX>
	$<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
	$<$<PLATFORM_ID:Darwin>:WEBRTC_MAC>
    NDEBUG
	MSC_EXPORTS
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra -Wpedantic
)
