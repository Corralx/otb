cmake_minimum_required (VERSION 3.0.2)

project (otb CXX)

set (RESOURCES_DIR "${CMAKE_SOURCE_DIR}/resources")
set (SHADER_DIR "${RESOURCES_DIR}/shaders")
set (CONFIG_FILE "${RESOURCES_DIR}/config.json")

set (
	SRC
	main.cpp
	utils.cpp
	embree.cpp
	occlusion.cpp
	rasterizer.cpp
	postprocess.cpp
	configuration.cpp
	binding_manager.cpp
	render_manager.cpp
	imgui_sdl_bridge.cpp
)

set (
	HEADER
	utils.hpp
	mesh.hpp
	embree.hpp
	image.hpp
	occlusion.hpp
	rasterizer.hpp
	postprocess.hpp
	configuration.hpp
	buffer_manager.hpp
	binding_manager.hpp
	render_manager.hpp
	material.hpp
	program.hpp
	imgui_sdl_bridge.hpp
)

set (
	COMMON
	${CMAKE_SOURCE_DIR}/TODO.md
	${CMAKE_SOURCE_DIR}/LICENSE.md
	${CMAKE_SOURCE_DIR}/README.md
)

set (
	SHADER
	${SHADER_DIR}/base.vs
	${SHADER_DIR}/base.fs
	${SHADER_DIR}/occlusion.vs
	${SHADER_DIR}/occlusion.fs
	${SHADER_DIR}/wireframe.vs
	${SHADER_DIR}/wireframe.gs
	${SHADER_DIR}/wireframe.fs
)

set (
	ALL_FILES
	${SRC}
	${HEADER}
)

foreach (FILE ${ALL_FILES})
  get_filename_component (PARENT_DIR "${FILE}" PATH)

  string (REPLACE "/" "\\" GROUP "${PARENT_DIR}")

  if ("${FILE}" MATCHES ".*\\.cpp")
	set (GROUP "Source Files\\${GROUP}")
  elseif ("${FILE}" MATCHES ".*\\.hpp")
	set (GROUP "Header Files\\${GROUP}")
  endif ()

  source_group ("${GROUP}" FILES "${FILE}")
endforeach ()

source_group ("Resources" FILES ${SHADER} ${CONFIG_FILE})

include_directories (${ELEKTRA_INCLUDE_PATH})
include_directories (${GL3W_INCLUDE_PATH})
include_directories (${GLM_INCLUDE_PATH})
include_directories (${GLI_INCLUDE_PATH})
include_directories (${CPPFORMAT_INCLUDE_PATH})
include_directories (${TINYOBJLOADER_INCLUDE_PATH})
include_directories (${IMGUI_INCLUDE_PATH})
include_directories (${SDL2_INCLUDE_PATH})
include_directories (${EMBREE_INCLUDE_PATH})
include_directories (${REMOTERY_INCLUDE_PATH})
include_directories (${STB_INCLUDE_PATH})
include_directories (${CHAISCRIPT_INCLUDE_PATH})
include_directories (${RAPIDJSON_INCLUDE_PATH})

add_executable (
	otb
	${SRC}
	${HEADER}
	${COMMON}
	${SHADER}
	${CONFIG_FILE}
)

set (
	OPENGL_LIB
	gl3w
	$<$<PLATFORM_ID:Windows>:opengl32>
)

add_definitions (-DNOMINMAX)

target_link_libraries (
	otb
	elektra 
	cppformat
	tinyobjloader
	imgui
	remotery
	${OPENGL_LIB}
	${SDL2_LIB_PATH}/SDL2.lib
	${SDL2_LIB_PATH}/SDL2main.lib
	${EMBREE_LIB_PATH}/embree.lib
)

set (MSVC_OPTIONS /MP /arch:AVX2 /bigobj)
set (GNU_OPTIONS -std=c++11 -arch=core-avx2)
set (CLANG_OPTIONS -std=c++11 -arch=core-avx2)

target_compile_options (
  otb PUBLIC
  "$<$<CXX_COMPILER_ID:MSVC>:${MSVC_OPTIONS}>"
  "$<$<CXX_COMPILER_ID:GNU>:${GNU_OPTIONS}>"
  "$<$<CXX_COMPILER_ID:Clang>:${CLANG_OPTIONS}>"
)

set (CLANG_WARNINGS -Weverything -pedantic -Werror -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-unknown-pragmas)
set (MSVC_WARNINGS /wd4068 /wd4201 /W4 /WX)
set (GNU_WARNINGS -Wall -Wextra -pedantic -Werror -Wno-pragmas -Wno-unknown-pragmas)

target_compile_options (
  otb PUBLIC 
  "$<$<CXX_COMPILER_ID:MSVC>:${MSVC_WARNINGS}>"
  "$<$<CXX_COMPILER_ID:GNU>:${GNU_WARNINGS}>"
  "$<$<CXX_COMPILER_ID:Clang>:${CLANG_WARNINGS}>"
)

set_property (TARGET otb PROPERTY LINK_FLAGS "/INCREMENTAL:NO")

set_property (TARGET otb PROPERTY RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set_property (TARGET otb PROPERTY PDB_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")

add_custom_command(TARGET otb POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                   ${RESOURCES_DIR} $<TARGET_FILE_DIR:otb>/resources)
