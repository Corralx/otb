cmake_minimum_required (VERSION 3.0.2)

project (imgui CXX)

set (
	SRC
	src/imgui.cpp
	src/imgui_draw.cpp
)

set (
	HEADERS
	include/imgui/imconfig.h
	include/imgui/imgui_internal.h
	include/imgui/stb_rect_pack.h
	include/imgui/stb_textedit.h
	include/imgui/stb_truetype.h
	include/imgui/imgui.h
)

add_library (imgui STATIC ${SRC} ${HEADERS})

include_directories (include/imgui)
include_directories (src)

target_compile_options (
  imgui PUBLIC
  "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-std=c++11>"
)

set_property (TARGET imgui PROPERTY LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set_property (TARGET imgui PROPERTY ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")
set_property (TARGET imgui PROPERTY PDB_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")
