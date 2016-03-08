#pragma once

#include "elektra/filesystem.hpp"

struct configuration
{
	elk::path shader_path;
	elk::path output_path;
};

extern configuration global_config;

bool init_configuration(const elk::path& path);
