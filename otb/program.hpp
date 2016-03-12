#pragma once

#include <cstdint>

struct base_program_t
{
	base_program_t() : id(0), color_location(0) {}

	uint32_t id;
	uint32_t color_location;
};

struct occlusion_program_t
{
	occlusion_program_t() : id(0), texture_location(0) {}

	uint32_t id;
	uint32_t texture_location;
};

struct wireframe_program_t
{
	wireframe_program_t() : id(0) {}

	uint32_t id;
};
