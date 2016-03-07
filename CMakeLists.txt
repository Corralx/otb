cmake_minimum_required (VERSION 3.0.2)

project (otb)

set (SDL2_INCLUDE_PATH "" CACHE PATH "SDL2 include path")
set (SDL2_LIB_PATH "" CACHE PATH "SDL2 library path")

set (EMBREE_PATH "C:/Program Files/Intel/Embree v2.8.0 x64" CACHE PATH "Intel Embree installation path")

set (GL3W_PATH ${CMAKE_SOURCE_DIR}/external/gl3w)
set (GLM_PATH ${CMAKE_SOURCE_DIR}/external/glm)
set (GLI_PATH ${CMAKE_SOURCE_DIR}/external/gli)
set (TCLAP_PATH ${CMAKE_SOURCE_DIR}/external/tclap)
set (ELEKTRA_PATH ${CMAKE_SOURCE_DIR}/external/elektra)
set (CPPFORMAT_PATH ${CMAKE_SOURCE_DIR}/external/cppformat)
set (TINYOBJLOADER_PATH ${CMAKE_SOURCE_DIR}/external/tinyobjloader)
set (IMGUI_PATH ${CMAKE_SOURCE_DIR}/external/imgui)
set (REMOTERY_PATH ${CMAKE_SOURCE_DIR}/external/remotery)
set (STB_PATH ${CMAKE_SOURCE_DIR}/external/stb)
set (CHAISCRIPT_PATH ${CMAKE_SOURCE_DIR}/external/chaiscript)

set (
	GL3W_INCLUDE_PATH
	${GL3W_PATH}/include
)

set (
	ELEKTRA_INCLUDE_PATH
	${ELEKTRA_PATH}/elektra/include
	${ELEKTRA_PATH}/elektra/src
	${ELEKTRA_PATH}/external
)

set (
	GLM_INCLUDE_PATH
	${GLM_PATH}
)

set (
	GLI_INCLUDE_PATH
	${GLI_PATH}
)

set (
	TCLAP_INCLUDE_PATH
	${TCLAP_PATH}
)

set (
	CPPFORMAT_INCLUDE_PATH
	${CPPFORMAT_PATH}
)

set (
	TINYOBJLOADER_INCLUDE_PATH
	${TINYOBJLOADER_PATH}
)

set (
	IMGUI_INCLUDE_PATH
	${IMGUI_PATH}/include
)

set (
	EMBREE_INCLUDE_PATH
	${EMBREE_PATH}/include
)

set (
	EMBREE_LIB_PATH
	${EMBREE_PATH}/lib
)

set (
	REMOTERY_INCLUDE_PATH
	${REMOTERY_PATH}
)

set (
	STB_INCLUDE_PATH
	${STB_PATH}
)

set (
	CHAISCRIPT_INCLUDE_PATH
	${CHAISCRIPT_PATH}
)

add_definitions (-DRMT_USE_OPENGL)

add_subdirectory (${GL3W_PATH})
add_subdirectory (${ELEKTRA_PATH})
add_subdirectory (${CPPFORMAT_PATH})
add_subdirectory (${IMGUI_PATH})
add_subdirectory (${TINYOBJLOADER_PATH})
add_subdirectory (${REMOTERY_PATH})
add_subdirectory (otb)
