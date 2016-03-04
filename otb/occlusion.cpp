#include "occlusion.hpp"
#include "mesh.hpp"
#include "utils.hpp"

#include "glm/glm.hpp"
#include "elektra/optional.hpp"

#include <thread>
#include <mutex>
#include <queue>
#include <vector>
#include <cstdint>

using image_f32 = image<image_format::F32>;
using image_u32 = image<image_format::U32>;

struct image_tile
{
	image_tile(uint32_t x, uint32_t y) : starting_x(x), starting_y(y) {}

	uint32_t starting_x;
	uint32_t starting_y;
};

static elk::optional<image_tile> get_next_tile(std::queue<image_tile>& queue)
{
	static std::mutex mutex;

	std::lock_guard<std::mutex> lock(mutex);

	if (queue.empty())
		return elk::nullopt;

	auto tile = queue.front();
	queue.pop();
	return tile;
}

static void process_tiles(std::queue<image_tile>& queue, embree::context& ctx, const mesh_t& mesh,
						  const occlusion_params& params, const image_u32& indices_map, image_f32& image)
{
	while (true)
	{
		auto tile_opt = get_next_tile(queue);
		if (!tile_opt)
			return;

		image_tile tile = tile_opt.value();

		auto& faces = mesh.faces();
		auto& positions = mesh.vertices();
		auto& tex_coords = mesh.texture_coords();
		auto& normals = mesh.normals();

		const uint32_t samples_per_pixel = params.quality * 8;

		for (uint32_t i = tile.starting_y; i < tile.starting_y + params.tile_height; ++i)
		{
			for (uint32_t j = tile.starting_x; j < tile.starting_x + params.tile_width; ++j)
			{
				const uint32_t tris_index = indices_map[i * image.width() + j];

				// Check if a triangle actually cover this pixel
				if (tris_index == std::numeric_limits<uint32_t>::max())
					continue;

				// Calculate UV coordinates for the center of the current pixel
				const glm::vec2 p_coord{ j / static_cast<float>(image.width()),
										 i / static_cast<float>(image.height()) };

				const uint32_t v0_index = faces[tris_index].v0;
				const uint32_t v1_index = faces[tris_index].v1;
				const uint32_t v2_index = faces[tris_index].v2;

				// Get UV coordinates for the vertices of the triangle which includes this pixel
				const glm::vec2 v0_coord{ tex_coords[v0_index].x,
										  tex_coords[v0_index].y };
				const glm::vec2 v1_coord{ tex_coords[v1_index].x,
										  tex_coords[v1_index].y };
				const glm::vec2 v2_coord{ tex_coords[v2_index].x ,
										  tex_coords[v2_index].y };

				// Use baricentric interpolation to obtain the position and normal of the given pixel when projected on the mesh
				// http://answers.unity3d.com/questions/383804/calculate-uv-coordinates-of-3d-point-on-plane-of-m.html
				const glm::vec2 p0_coord = v0_coord - p_coord;
				const glm::vec2 p1_coord = v1_coord - p_coord;
				const glm::vec2 p2_coord = v2_coord - p_coord;

				const float area_tris = glm::length(glm::cross(glm::vec3(p0_coord - p1_coord, .0f), glm::vec3(p0_coord - p2_coord, .0f)));
				const float area0 = glm::length(glm::cross(glm::vec3(p1_coord, .0f), glm::vec3(p2_coord, .0f))) / area_tris;
				const float area1 = glm::length(glm::cross(glm::vec3(p2_coord, .0f), glm::vec3(p0_coord, .0f))) / area_tris;
				const float area2 = glm::length(glm::cross(glm::vec3(p0_coord, .0f), glm::vec3(p1_coord, .0f))) / area_tris;

				const glm::vec3 v0{ positions[v0_index].x,
									positions[v0_index].y,
									positions[v0_index].z };

				const glm::vec3 v1{ positions[v1_index].x,
									positions[v1_index].y,
									positions[v1_index].z };

				const glm::vec3 v2{	positions[v2_index].x,
									positions[v2_index].y,
									positions[v2_index].z };

				const glm::vec3 p = v0 * area0 + v1 * area1 + v2 * area2;

				const glm::vec3 n0{ normals[v0_index].x,
									normals[v0_index].y,
									normals[v0_index].z };

				const glm::vec3 n1{ normals[v1_index].x,
									normals[v1_index].y,
									normals[v1_index].z };

				const glm::vec3 n2{ normals[v2_index].x,
									normals[v2_index].y,
									normals[v2_index].z };

				// Setting smooth_inter_normal to false will just take the mean value of the normals
				glm::vec3 n;
				if (params.smooth_normal_interpolation)
					n = n0 * area0 + n1 * area1 + n2 * area2;
				else
					n = (n0 + n1 + n2) / 3.f;

				// Generate the rays in groups of 8, to make use of Embree AVX2 capabilities
				uint32_t num_hit = 0;
				float occlusion = .0f;
				for (uint32_t q = 0; q < params.quality; ++q)
				{
					embree::ray ray;

					for (uint32_t ray_id = 0; ray_id < 8; ++ray_id)
					{
						ray.positions[ray_id] = p;

						const glm::vec3 dir = cosine_weighted_hemisphere_sample(n);
						ray.directions[ray_id] = dir;
					}

					auto result = ctx.intersect(ray, params.max_distance, params.min_distance);

					// Sum up occlusion for each hit accounting for attenuation
					for (uint32_t ray_id = 0; ray_id < 8; ++ray_id)
					{
						if (result.ids[ray_id] != embree::NO_HIT_ID)
						{
							occlusion += 1.f - saturate(result.distances[ray_id] / params.max_distance);
							++num_hit;
						}
					}
				}

				if (num_hit > 0)
				{
					occlusion /= samples_per_pixel;
					occlusion /= params.linear_attenuation;
					occlusion = std::pow(occlusion, params.quadratic_attenuation);
					image[i * image.width() + j] = saturate(occlusion);
				}
			}
		}
	}
}

static void generate_occlusion_helper(embree::context& ctx, const mesh_t& mesh, const occlusion_params& params,
									  const image_u32& indices_map, image_f32& image, std::promise<void> promise)
{
	std::vector<std::thread> workers;
	std::queue<image_tile> queue;

	assert(params.worker_num > 0);
	assert(image.width() % params.tile_width == 0);
	assert(image.height() % params.tile_height == 0);
	assert(params.quality > 0);

	const uint32_t num_tile_width = image.width() / params.tile_width;
	const uint32_t num_tile_height = image.height() / params.tile_height;

	for (uint32_t i = 0; i < num_tile_height; ++i)
		for (uint32_t j = 0; j < num_tile_width; ++j)
			queue.emplace(j * params.tile_width, i * params.tile_height);

	for (uint32_t w = 0; w < params.worker_num; ++w)
		workers.push_back(std::thread(process_tiles, std::ref(queue), std::ref(ctx),
									  std::ref(mesh), params, std::ref(indices_map), std::ref(image)));

	for (auto& w : workers)
		w.join();

	promise.set_value_at_thread_exit();
}

/*
std::future<void> generate_occlusion_map(const mesh_t& mesh, occlusion_params params, image_f32& image)
{
	std::promise<void> promise;
	std::future<void> future = promise.get_future();

	std::thread([](const mesh_t& mesh, occlusion_params params, image_f32& image, std::promise<void> promise)
	{
		generate_occlusion_helper(mesh, params, image, std::move(promise));
	}, std::ref(mesh), params, std::ref(image), std::move(promise)).detach();

	return std::move(future);
}
*/

std::future<void> generate_occlusion_map(embree::context& ctx, const mesh_t& mesh, const occlusion_params& params,
										 const image_u32& indices_map, image_f32& image)
{
	return async_apply(generate_occlusion_helper, std::ref(ctx), std::ref(mesh),
					   std::ref(params), std::ref(indices_map), std::ref(image));
}
