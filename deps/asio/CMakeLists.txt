cmake_minimum_required(VERSION 3.16)

project(asio LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(${PROJECT_NAME} STATIC
	src/asio/src/asio.cpp
	src/asio/src/asio_ssl.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC
	src/asio/include
)

find_package(OpenSSL)

target_link_libraries(${PROJECT_NAME} PRIVATE
	${OPENSSL_LIBRARIES}
)

target_compile_definitions(${PROJECT_NAME} PUBLIC
	ASIO_STANDALONE
	ASIO_SEPARATE_COMPILATION
)
