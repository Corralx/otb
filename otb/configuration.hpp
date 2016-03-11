#pragma once

#include "elektra/filesystem.hpp"

#include <string>

struct configuration
{
	elk::path shader_path;
	elk::path output_path;

	elk::path base_vertex_shader;
	elk::path base_fragment_shader;

	elk::path occlusion_vertex_shader;
	elk::path occlusion_fragment_shader;

	elk::path wireframe_vertex_shader;
	elk::path wireframe_geometry_shader;
	elk::path wireframe_fragment_shader;
};

extern configuration global_config;

bool init_configuration(const elk::path& path);
