cmake_minimum_required (VERSION 3.0.2)

project (gl3w C)

include_directories (include)

set (SRC src/gl3w.c)

set (
	HEADER
	include/GL/gl3w.h
	include/GL/glcorearb.h
	"$<$<PLATFORM_ID:Windows>:include/GL/wglext.h>"
	"$<$<PLATFORM_ID:Linux>:include/GL/glxext.h>"
)

add_library (gl3w STATIC ${SRC} ${HEADER})

if (UNIX)
  target_link_libraries (gl3w dl)
endif()

set_property (TARGET gl3w PROPERTY LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set_property (TARGET gl3w PROPERTY ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")
set_property (TARGET gl3w PROPERTY PDB_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")
