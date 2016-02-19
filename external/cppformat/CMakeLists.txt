cmake_minimum_required (VERSION 3.0.2)

project (cppformat CXX)

set (SRC cppformat/format.cc)
set (HEADERS cppformat/format.h)

add_library (cppformat STATIC ${SRC} ${HEADERS})

target_compile_options (
  cppformat PUBLIC
  "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-std=c++11>"
)

target_compile_definitions (cppformat PUBLIC FMT_USE_VARIADIC_TEMPLATES)

set_property (TARGET cppformat PROPERTY LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set_property (TARGET cppformat PROPERTY ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")
set_property (TARGET cppformat PROPERTY PDB_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")
