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

set_source_files_properties(${SOURCE_FILES}
    PROPERTIES COMPILE_FLAGS
    -Wall -Wextra -Wpedantic
)

add_library(${PROJECT_NAME} ${SOURCES})

target_sources(${PROJECT_NAME} PRIVATE
    src/lib.h
    src/lib.cpp
    src/peer_connection_factory.cpp
)

add_subdirectory(deps/fmt)
add_subdirectory(deps/libmediasoupclient)

target_include_directories(${PROJECT_NAME} PUBLIC
    deps/fmt/include
    deps/libmediasoupclient/include
    deps/libmediasoupclient/deps/libsdptransform/include
    deps/webrtc/include
    deps/webrtc/include/third_party/abseil-cpp
    deps/webrtc/include/third_party/libyuv/include
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
