cmake_minimum_required(VERSION 3.16)

project(libnet LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(${PROJECT_NAME} STATIC ${SOURCES})

target_sources(${PROJECT_NAME} PRIVATE
	include/net/http_client.hpp
	include/net/protoo.hpp
	src/http_client.cpp
	src/protoo.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)

target_link_libraries(${PROJECT_NAME} PUBLIC
	libcommon
	libhv
	OpenSSL::SSL
	OpenSSL::Crypto
	$<$<NOT:$<PLATFORM_ID:Windows>>:pthread>
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra -Wpedantic
)
