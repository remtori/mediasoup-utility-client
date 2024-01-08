cmake_minimum_required(VERSION 3.16)

project(load_test_bot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME} ${SOURCES})

target_sources(${PROJECT_NAME} PRIVATE
	src/conference_manager.hpp
	src/conference_manager.cpp
	src/conference.hpp
	src/conference.cpp
	src/consumer.hpp
	src/main.cpp
	src/timer_event_loop.hpp
	src/timer_event_loop.cpp
	src/ui.hpp
	src/ui.cpp
	src/viewer_manager.hpp
	src/viewer_manager.cpp
	src/viewer.hpp
	src/viewer.cpp
)

include(FetchContent)

FetchContent_Declare(ftxui
	GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
	GIT_TAG v4.1.1
)

FetchContent_GetProperties(ftxui)
if(NOT ftxui_POPULATED)
	FetchContent_Populate(ftxui)
	add_subdirectory(${ftxui_SOURCE_DIR} ${ftxui_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    msc
	common
	argparse
	ftxui::screen
	ftxui::dom
	ftxui::component
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
	$<$<PLATFORM_ID:Windows>:NOMINMAX>
	$<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra -Wpedantic
)
