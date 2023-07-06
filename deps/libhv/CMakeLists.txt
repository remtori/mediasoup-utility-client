cmake_minimum_required(VERSION 3.16)

SET(LIB_FILE ${CMAKE_CURRENT_SOURCE_DIR}/src/build/lib/libhv_static.a)

add_custom_command(OUTPUT ${LIB_FILE}
                   COMMAND ${CMAKE_COMMAND} -GNinja -Bbuild -DWITH_OPENSSL=ON
				   COMMAND ${CMAKE_COMMAND} --build build
				   COMMAND ${CMAKE_COMMAND} -E copy_directory build/include ../include
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_custom_target(libhv_build DEPENDS ${LIB_FILE})

add_library(libhv STATIC IMPORTED GLOBAL)

set_target_properties(
	libhv
		PROPERTIES
		IMPORTED_LOCATION ${LIB_FILE}
		INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_dependencies(libhv libhv_build)
