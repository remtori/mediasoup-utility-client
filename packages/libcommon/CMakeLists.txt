cmake_minimum_required(VERSION 3.16)

project(libcommon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(${PROJECT_NAME} STATIC ${SOURCES})

target_sources(${PROJECT_NAME} PRIVATE
	include/common/crc32.hpp
	include/common/executor.hpp
	include/common/json.hpp
	include/common/logger.hpp
	src/crc32.cpp
	src/logger.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PUBLIC
	fmt::fmt
	$<$<NOT:$<PLATFORM_ID:Windows>>:pthread>
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra -Wpedantic
)

set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
