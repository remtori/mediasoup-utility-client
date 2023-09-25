cmake_minimum_required(VERSION 3.16)

add_compile_options(
	$<$<NOT:$<PLATFORM_ID:Windows>>:-fPIE>
	$<$<NOT:$<PLATFORM_ID:Windows>>:-fPIC>
	-Wno-deprecated-declarations
)

set(LIBWEBRTC_INCLUDE_PATH "${PROJECT_SOURCE_DIR}/deps/webrtc/include" CACHE STRING "" FORCE)
set(LIBWEBRTC_BINARY_PATH "${PROJECT_SOURCE_DIR}/deps/webrtc/prebuilt/x86_64-windows" CACHE STRING "" FORCE)

add_subdirectory(fmt)
add_subdirectory(libmediasoupclient)
add_subdirectory(libhv)
