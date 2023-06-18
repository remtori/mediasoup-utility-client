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
)

add_subdirectory(
    deps/libmediasoupclient
)

target_include_directories(${PROJECT_NAME} PUBLIC
    deps/libmediasoupclient/include
    deps/libmediasoupclient/deps/libsdptransform/include
    deps/webrtc/include
    deps/webrtc/include/third_party/abseil-cpp
    deps/webrtc/include/third_party/libyuv/include
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    mediasoupclient
)
