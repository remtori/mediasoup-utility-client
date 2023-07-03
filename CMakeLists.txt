cmake_minimum_required(VERSION 3.16)

project(mediasoup_utility_client LANGUAGES CXX)

# Honor visibility properties for all target
cmake_policy(SET CMP0063 NEW)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)

add_subdirectory(deps)
add_subdirectory(msc)
add_subdirectory(stream-writer)
