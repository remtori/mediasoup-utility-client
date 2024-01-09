cmake_minimum_required(VERSION 3.16)

project(msc_stream_writer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME} ${SOURCES})

target_sources(${PROJECT_NAME} PRIVATE
	src/main.cpp
    src/video-writer.hpp
    src/video-writer.cpp
)

find_package(OpenCV REQUIRED)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${OpenCV_LIBS}
    msc
	common
)

target_compile_definitions(${PROJECT_NAME} PRIVATE

)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra -Wpedantic
)
