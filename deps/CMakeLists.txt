cmake_minimum_required(VERSION 3.16)

add_compile_options(
	$<$<NOT:$<PLATFORM_ID:Windows>>:-fPIE>
	$<$<NOT:$<PLATFORM_ID:Windows>>:-fPIC>
	-Wno-deprecated-declarations
)

set(PLATFORM_TARGET "x86_64-unknown-linux")
if (WIN32)
    set(PLATFORM_TARGET "x86_64-windows")
endif()

set(LIBWEBRTC_INCLUDE_PATH "${PROJECT_SOURCE_DIR}/deps/webrtc/include" CACHE STRING "" FORCE)
set(LIBWEBRTC_BINARY_PATH "${PROJECT_SOURCE_DIR}/deps/webrtc/prebuilt/${PLATFORM_TARGET}" CACHE STRING "" FORCE)

add_library(argparse INTERFACE)
target_include_directories(argparse INTERFACE
	argparse/include
)

add_subdirectory(fmt)
add_subdirectory(libmediasoupclient)
add_subdirectory(libhv)
