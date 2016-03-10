#pragma once

#include <cstdint>
#include <future>

#include "image.hpp"
#include "embree.hpp"

class mesh_t;

struct occlusion_params
{
	// Rays are packet by 8, so the number of samples per pixel is quality * 8
	uint32_t quality = 1;

	// The min and max distance the system search for an occluder
	float min_distance = .0001f;
	float max_distance = 100.f;

	// TODO(Corralx): Let the user personalize the occlusion calculation through a lambda?
	// The attenuation parameter used to calculate final occlusion
	float quadratic_attenuation = 1.f;
	float linear_attenuation = 1.f;

	// The number of workers (aka threads) and the size of the tile each worker works onto
	// NOTE(Corralx): The tile size must always be a divisor of the occlusion map size!
	uint32_t tile_width = 64;
	uint32_t tile_height = 64;
	uint8_t worker_num = 8;

	// Setting this to false disable barycentric interpolation for the normals and use the mean instead
	bool smooth_normal_interpolation = true;
};

// When the future is ready, the image contains the generated occlusion map
/* NOTE(Corralx): Only the pixels covered by the UV unwrap are overwritten
   if a default value is needed, call initialize(...) on the image before submitting */
std::future<void> generate_occlusion_map(embree::context& ctx, const mesh_t& mesh, const occlusion_params& params,
										 const image<pixel_format::U32>& indices_map, image<pixel_format::F32>& image);
