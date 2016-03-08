#include "configuration.hpp"

#include "elektra/file_io.hpp"

#pragma warning(push, 0)
#include "rapidjson/document.h"
#pragma warning(pop)

configuration global_config;

static constexpr const char* JSON_SHADER_PATH = "shader_path";
static constexpr const char* JSON_OUTPUT_PATH = "output_path";

bool init_configuration(const elk::path& path)
{
	auto config = elk::get_content_of_file(path);
	if (!config)
		return false;

	rapidjson::Document json;
	json.Parse(config.value().c_str());

	if (json.HasParseError())
		return false;

	if (!json.HasMember(JSON_SHADER_PATH) || !json[JSON_SHADER_PATH].IsString())
		return false;
	global_config.shader_path = json[JSON_SHADER_PATH].GetString();
	
	if (!json.HasMember(JSON_OUTPUT_PATH) || !json[JSON_OUTPUT_PATH].IsString())
		return false;
	global_config.output_path = json[JSON_OUTPUT_PATH].GetString();

	return true;
}
