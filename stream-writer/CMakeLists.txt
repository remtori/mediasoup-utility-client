cmake_minimum_required(VERSION 3.16)

project(msc_stream_writer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME} ${SOURCES})

target_sources(${PROJECT_NAME} PRIVATE
	src/main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
	${CMAKE_SOURCE_DIR}/msc/include
	${CMAKE_SOURCE_DIR}/deps/websocketpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
	asio
    msc
	$<$<NOT:$<PLATFORM_ID:Windows>>:pthread>
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
	ASIO_STANDALONE
	ASIO_SEPARATE_COMPILATION
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra -Wpedantic
)
