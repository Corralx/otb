#include "configuration.hpp"

#include "elektra/file_io.hpp"

#pragma warning(push, 0)
#include "rapidjson/document.h"
#pragma warning(pop)

configuration global_config;

static constexpr const char* JSON_SHADER_PATH = "shader_path";
static constexpr const char* JSON_OUTPUT_PATH = "output_path";
static constexpr const char* JSON_BASE_VERTEX_PATH = "base_vertex_shader";
static constexpr const char* JSON_BASE_FRAGMENT_PATH = "base_fragment_shader";
static constexpr const char* JSON_OCCLUSION_VERTEX_PATH = "occlusion_vertex_shader";
static constexpr const char* JSON_OCCLUSION_FRAGMENT_PATH = "occlusion_fragment_shader";
static constexpr const char* JSON_WIREFRAME_VERTEX_PATH = "wireframe_vertex_shader";
static constexpr const char* JSON_WIREFRAME_GEOMETRY_PATH = "wireframe_geometry_shader";
static constexpr const char* JSON_WIREFRAME_FRAGMENT_PATH = "wireframe_fragment_shader";

#define GET_JSON_STRING(var, member) \
if (!json.HasMember(var) || !json[var].IsString()) \
	return false; \
global_config.member = json[var].GetString()

bool init_configuration(const elk::path& path)
{
	auto config = elk::get_content_of_file(path);
	if (!config)
		return false;

	rapidjson::Document json;
	json.Parse(config.value().c_str());

	if (json.HasParseError())
		return false;

	GET_JSON_STRING(JSON_SHADER_PATH, shader_path);
	GET_JSON_STRING(JSON_OUTPUT_PATH, output_path);
	GET_JSON_STRING(JSON_BASE_VERTEX_PATH, base_vertex_shader);
	GET_JSON_STRING(JSON_BASE_FRAGMENT_PATH, base_fragment_shader);
	GET_JSON_STRING(JSON_OCCLUSION_VERTEX_PATH, occlusion_vertex_shader);
	GET_JSON_STRING(JSON_OCCLUSION_FRAGMENT_PATH, occlusion_fragment_shader);
	GET_JSON_STRING(JSON_WIREFRAME_VERTEX_PATH, wireframe_vertex_shader);
	GET_JSON_STRING(JSON_WIREFRAME_GEOMETRY_PATH, wireframe_geometry_shader);
	GET_JSON_STRING(JSON_WIREFRAME_FRAGMENT_PATH, wireframe_fragment_shader);

	return true;
}

#undef GET_JSON_STRING
