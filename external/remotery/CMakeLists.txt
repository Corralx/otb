cmake_minimum_required (VERSION 3.0.2)

project (remotery CXX)

set (SRC remotery/remotery.c)
set (HEADERS remotery/remotery.h)

add_library (remotery STATIC ${SRC} ${HEADERS})

add_definitions (-DRMT_USE_OPENGL)

set_property (TARGET remotery PROPERTY LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set_property (TARGET remotery PROPERTY ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")
set_property (TARGET remotery PROPERTY PDB_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")
