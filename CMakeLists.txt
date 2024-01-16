cmake_minimum_required(VERSION 3.16)

project(mediasoup_utility_client LANGUAGES CXX)

# Honor visibility properties for all target
cmake_policy(SET CMP0063 NEW)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
# set(CMAKE_BUILD_TYPE RelWithDebInfo)

set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# --------- Setup the Executable output Directory -------------
if(NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY
            ${${PROJECT_NAME} _BINARY_DIR}/bin
            CACHE PATH "Single Directory for all static libraries")
endif()

add_subdirectory(deps)
add_subdirectory(packages/libcommon)
add_subdirectory(packages/libnet)
add_subdirectory(packages/libmsc)
add_subdirectory(packages/load-test-bot)
add_subdirectory(packages/reencoder)
# add_subdirectory(packages/stream-writer)
