cmake_minimum_required(VERSION 3.16)

project(ms-client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LIBWEBRTC_INCLUDE_PATH "${PROJECT_SOURCE_DIR}/deps/webrtc/include")
set(LIBWEBRTC_BINARY_PATH "${PROJECT_SOURCE_DIR}/deps/webrtc")

set_source_files_properties(${SOURCE_FILES}
    PROPERTIES COMPILE_FLAGS
    -Wall -Wextra -Wpedantic
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_sources(${PROJECT_NAME} PRIVATE
    src/main.cpp
)

add_subdirectory(
    deps/libmediasoupclient
)

target_include_directories(${PROJECT_NAME} PUBLIC
    deps/libmediasoupclient/include
    deps/libmediasoupclient/deps/libsdptransform/include
    deps/webrtc/include
    deps/webrtc/include/third_party/abseil-cpp
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    mediasoupclient
)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lpthread -lX11 -ldl")
