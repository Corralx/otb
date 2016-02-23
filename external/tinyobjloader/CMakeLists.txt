cmake_minimum_required (VERSION 3.0.2)

project (tinyobjloader CXX)

set (SRC tinyobjloader/tiny_obj_loader.cc)
set (HEADERS tinyobjloader/tiny_obj_loader.h)

add_library (tinyobjloader STATIC ${SRC} ${HEADERS})

add_definitions (-D_CRT_SECURE_NO_WARNINGS)

target_compile_options (
  tinyobjloader PUBLIC
  "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-std=c++11>"
)

set_property (TARGET tinyobjloader PROPERTY LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set_property (TARGET tinyobjloader PROPERTY ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")
set_property (TARGET tinyobjloader PROPERTY PDB_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")
