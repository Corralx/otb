cmake_minimum_required (VERSION 3.0.2)

project (lodepng CXX)

set (SRC lodepng/lodepng.cpp)
set (HEADERS lodepng/lodepng.h)

add_library (lodepng STATIC ${SRC} ${HEADERS})

set_property (TARGET lodepng PROPERTY LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set_property (TARGET lodepng PROPERTY ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")
set_property (TARGET lodepng PROPERTY PDB_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")
