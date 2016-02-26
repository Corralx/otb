#include <iostream>
#include <cstdint>
#include <random>
#include <algorithm>
#include <limits>

#include "imgui/imgui.h"
#include "imgui_impl_sdl_gl3.hpp"
#pragma warning (push, 0)
#include "glm/glm.hpp"
#include "gli/gli.hpp"
#pragma warning (pop)
#include "GL/gl3w.h"
#include "SDL2/SDL.h"
#include "tinyobjloader/tiny_obj_loader.h"
#include "elektra/filesystem.hpp"

#pragma warning (disable : 4324)
#include "embree2/rtcore.h"
#include "embree2/rtcore_ray.h"
#include <xmmintrin.h>
#include <pmmintrin.h>

static constexpr uint32_t APP_WIDTH = 1280;
static constexpr uint32_t APP_HEIGHT = 720;

struct embree_vertex
{
	float x;
	float y;
	float z;
	float _padding;
};

struct embree_triangle
{
	int32_t v0;
	int32_t v1;
	int32_t v2;
};

struct RTCORE_ALIGN(16) ray_mask
{
	uint32_t _[8];
};

double random_float()
{
	static std::random_device rd;
	static std::mt19937 gen(rd());
	static std::uniform_real_distribution<> dist(.0f, 1.f);

	return dist(gen);
}

glm::vec3 cosine_weighted_hemisphere(glm::vec3 n)
{
	double Xi1 = random_float();
	double Xi2 = random_float();

	double  theta = acos(sqrt(1.0 - Xi1));
	double  phi = 2.0 * 3.1415926535897932384626433832795 * Xi2;

	float xs = static_cast<float>(sin(theta) * cos(phi));
	float ys = static_cast<float>(cos(theta));
	float zs = static_cast<float>(sin(theta) * sin(phi));

	glm::vec3 h = n;
	if (abs(h.x) <= abs(h.y) && abs(h.x) <= abs(h.z))
		h.x = 1.0;
	else if (abs(h.y) <= abs(h.x) && abs(h.y) <= abs(h.z))
		h.y = 1.0;
	else
		h.z = 1.0;


	glm::vec3 x = glm::normalize(glm::cross(h, n));
	glm::vec3 z = glm::normalize(glm::cross(x, n));

	glm::vec3 direction = xs * x + ys * n + zs * z;
	return glm::normalize(direction);
}

double normal_distribution()
{
	static std::random_device rd;
	static std::mt19937 gen(rd());
	static std::uniform_real_distribution<> dist(.0f, .07f);

	return dist(gen);
}

template<typename T>
T saturate(T value)
{
	return std::min(1.f, std::max(.0f, value));
}

bool same_side(const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& a, const glm::vec2& b)
{
	auto b_minus_a = glm::vec3(b.x - a.x, b.y - a.y, .0f);
	auto p1_minus_a = glm::vec3(p1.x - a.x, p1.y - a.y, .0f);
	auto p2_minus_a = glm::vec3(p2.x - a.x, p2.y - a.y, .0f);

	auto cp1 = glm::cross(b_minus_a, p1_minus_a);
	auto cp2 = glm::cross(b_minus_a, p2_minus_a);

	if (glm::dot(cp1, cp2) >= 0)
		return true;
	else
		return false;
}

bool point_in_tris(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b, const glm::vec2& c)
{
	return same_side(p, a, b, c) && same_side(p, b, a, c) && same_side(p, c, a, b);
}

struct edge_t
{
	edge_t() = delete;
	edge_t(const glm::vec2& _v0, const glm::vec2& _v1) : v0(_v0), v1(_v1)
	{
		// Ordering edges so that v0 is always the lower end
		if (_v0.y > _v1.y)
		{
			v0 = _v1;
			v1 = _v0;
		}
	}

	glm::vec2 v0;
	glm::vec2 v1;
};

struct span_t
{
	span_t() = delete;
	span_t(uint32_t _x0, uint32_t _x1) : x0(_x0), x1(_x1)
	{
		// Ordering spans so that x0 is always the lower end
		if (_x0 > _x1)
		{
			x0 = _x1;
			x1 = _x0;
		}
	}

	uint32_t x0;
	uint32_t x1;
};

int main(int, char*[])
{
	// Setting the CPU register flags for Embree
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
	_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);

	ray_mask _ray_mask;
	for (uint32_t& ui : _ray_mask._)
		ui = 0xFFFFFFFF;

	if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
	{
		std::cout << "SDL initialization error: " << SDL_GetError() << std::endl;
		return 1;
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

	SDL_DisplayMode current;
	SDL_GetCurrentDisplayMode(0, &current);
	SDL_Window *window = SDL_CreateWindow("Occlusion and Translucency Baker", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
										  APP_WIDTH, APP_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
	SDL_GLContext glcontext = SDL_GL_CreateContext(window);
	gl3wInit();

	ImGui_ImplSdlGL3_Init(window);

	glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
	glClearColor(1.f, .0f, .0f, 1.f);
	SDL_GL_SetSwapInterval(0);

	std::vector<tinyobj::shape_t> shapes;
	{
		elk::path mesh_path("test/test.obj");
		std::vector<tinyobj::material_t> materials;
		std::string error;
		bool res = tinyobj::LoadObj(shapes, materials, error, mesh_path.c_str(), nullptr, true);

		if (!res)
			std::cerr << "Error: " << error;

		std::cout << "Shapes found: " << shapes.size() << std::endl;
		
		for (uint32_t i = 0; i < shapes.size(); ++i)
		{
			size_t num_vertices = shapes[i].mesh.positions.size() / 3;
			size_t num_normals = shapes[i].mesh.normals.size() / 3;
			size_t num_tex_coords = shapes[i].mesh.texcoords.size() / 2;
			size_t num_indices = shapes[i].mesh.indices.size() / 3;

			assert(num_vertices == num_normals && num_vertices == num_tex_coords);
			
			std::cout << std::endl << "Shape " << i << " (" << shapes[i].name << ")" << std::endl;
			std::cout << "Vertices: " << num_vertices << std::endl;
			std::cout << "Normals: " << num_normals << std::endl;
			std::cout << "Texture coords: " << num_tex_coords << std::endl;
			std::cout << "Indices: " << num_indices << std::endl;
		}
	}

	std::cout << std::endl;

	auto embree_device = rtcNewDevice();
	if (!embree_device)
		std::cerr << "Error initializing Embree!" << std::endl;

	auto embree_scene = rtcDeviceNewScene(embree_device, RTC_SCENE_STATIC | RTC_SCENE_INCOHERENT |
										  RTC_SCENE_HIGH_QUALITY | RTC_SCENE_ROBUST, RTC_INTERSECT8);
	if (!embree_scene)
		std::cerr << "Error creating Embree scene!" << std::endl;

	rtcDeviceSetMemoryMonitorFunction(embree_device, [](const ssize_t bytes, const bool)
	{
		if (bytes > 0)
			std::cout << "EMBREE: Allocating " << bytes << " bytes of memory!" << std::endl;
		else
			std::cout << "EMBREE: Deallocating " << bytes << " bytes of memory!" << std::endl;
		return true;
	});

	rtcSetProgressMonitorFunction(embree_scene, [](void*, const double n)
	{
		std::cout << "EMBREE: Building data structure: " << (n * 100) << "%!" << std::endl;
		return true;
	}, nullptr);

	std::vector<uint32_t> embree_meshes;
	for (auto& s : shapes)
	{
		size_t num_indices = s.mesh.indices.size() / 3;
		size_t num_vertices = s.mesh.positions.size() / 3;
		uint32_t id = rtcNewTriangleMesh(embree_scene, RTC_GEOMETRY_STATIC, num_indices, num_vertices);
		embree_meshes.push_back(id);

		auto& vertices = s.mesh.positions;
		embree_vertex* embree_vertices = (embree_vertex*)rtcMapBuffer(embree_scene, id, RTC_VERTEX_BUFFER);
		for (uint32_t i = 0; i < num_vertices; ++i)
		{
			embree_vertices[i].x = vertices[i * 3 + 0];
			embree_vertices[i].y = vertices[i * 3 + 1];
			embree_vertices[i].z = vertices[i * 3 + 2];
			embree_vertices[i]._padding = .0f;
		}
		rtcUnmapBuffer(embree_scene, id, RTC_VERTEX_BUFFER);

		auto& indices = s.mesh.indices;
		embree_triangle* embree_triangles = (embree_triangle*)rtcMapBuffer(embree_scene, id, RTC_INDEX_BUFFER);
		for (uint32_t i = 0; i < num_indices; ++i)
		{
			embree_triangles[i].v0 = indices[i * 3 + 0];
			embree_triangles[i].v1 = indices[i * 3 + 1];
			embree_triangles[i].v2 = indices[i * 3 + 2];
		}
		rtcUnmapBuffer(embree_scene, id, RTC_INDEX_BUFFER);
	}

	rtcCommit(embree_scene);

	auto embree_error = rtcDeviceGetError(embree_device);
	if (embree_error != RTC_NO_ERROR)
		std::cerr << "Error loading Embree scene!" << std::endl;
	else
		std::cout << "Embree scene initialized correctly!" << std::endl;
	
	const uint32_t mesh_index = 1;
	const uint32_t occlusion_map_width = 512;
	const uint32_t occlusion_map_height = occlusion_map_width;

	// Step 1: Find the triangle index covering each pixel in the occlusion map
	uint32_t* indices_map;
	{
		const uint8_t ssaa_scale = 2;
		const uint32_t width = occlusion_map_width * ssaa_scale;
		const uint32_t height = width;

		const tinyobj::mesh_t& mesh = shapes[mesh_index].mesh;
		const size_t triangle_num = mesh.indices.size() / 3;
		
		// Allocate supersampled index map and set an invalid value used as sentinel
		uint32_t* index_map = new uint32_t[width * height];
		memset(index_map, 255, width * height * sizeof(uint32_t));

		// For each triangle, lookup the pixel it's covering in the occlusion map through triangle scan conversion
		// http://joshbeam.com/articles/triangle_rasterization/
		// http://www.sunshine2k.de/coding/java/TriangleRasterization/TriangleRasterization.html
		for (uint32_t tris_index = 0; tris_index < triangle_num; ++tris_index)
		{
			// Vertices index of current triangle
			const uint32_t v0_index = mesh.indices[tris_index * 3 + 0];
			const uint32_t v1_index = mesh.indices[tris_index * 3 + 1];
			const uint32_t v2_index = mesh.indices[tris_index * 3 + 2];

			// UV coordinates of current triangle
			const glm::vec2 v0_coord{ mesh.texcoords[v0_index * 2 + 0],
									  mesh.texcoords[v0_index * 2 + 1] };
			const glm::vec2 v1_coord{ mesh.texcoords[v1_index * 2 + 0],
									  mesh.texcoords[v1_index * 2 + 1] };
			const glm::vec2 v2_coord{ mesh.texcoords[v2_index * 2 + 0] ,
									  mesh.texcoords[v2_index * 2 + 1] };

			// Pixel coordinates pf current triangle
			const glm::vec2 v0_pixel{ v0_coord.x * width + 0.5,
									  v0_coord.y * height + 0.5 };
			const glm::vec2 v1_pixel{ v1_coord.x * width + 0.5,
									  v1_coord.y * height + 0.5 };
			const glm::vec2 v2_pixel{ v2_coord.x * width + 0.5,
									  v2_coord.y * height + 0.5 };

			// Current triangle edges
			const edge_t edges[3] =
			{
				{ v0_pixel, v1_pixel },
				{ v1_pixel, v2_pixel },
				{ v2_pixel, v0_pixel }
			};

			// Find the longer edge vertically
			uint32_t long_edge_index = 0;
			{
				float max_length = 0;
				for (uint32_t edge_index = 0; edge_index < 3; ++edge_index)
				{
					float length = edges[edge_index].v1.y - edges[edge_index].v0.y;
					if (length > max_length)
					{
						max_length = length;
						long_edge_index = edge_index;
					}
				}
			}

			const edge_t long_edge = edges[long_edge_index];
			const float long_edge_x_extent = long_edge.v1.x - long_edge.v0.x;
			const float long_edge_y_extent = long_edge.v1.y - long_edge.v0.y;

			// For each short edge, get all the pixel between it and the long edge
			for (uint32_t short_offset = 0; short_offset < 2; ++short_offset)
			{
				const uint32_t short_edge_index = (long_edge_index + 1 + short_offset) % 3;
				const edge_t short_edge = edges[short_edge_index];

				// Split the long segment to just account for the part as high as the short one
				const float x0_factor = (short_edge.v0.y - long_edge.v0.y) / long_edge_y_extent;
				const float x1_factor = (short_edge.v1.y - long_edge.v0.y) / long_edge_y_extent;
				const float long_segment_x0 = long_edge.v0.x + long_edge_x_extent * x0_factor;
				const float long_segment_x1 = long_edge.v0.x + long_edge_x_extent * x1_factor;
				const edge_t long_segment
				{
					{ long_segment_x0, short_edge.v0.y },
					{ long_segment_x1, short_edge.v1.y }
				};

				// Check if an edge is parallel to the X axis because it doesn't contribute
				// Both edges are actually checked to account for degenerates triangle
				const float long_edge_y_diff = long_segment.v1.y - long_segment.v0.y;
				if (std::abs(long_edge_y_diff) < .0001f)
					continue;

				const float short_edge_y_diff = short_edge.v1.y - short_edge.v0.y;
				if (short_edge_y_diff < .0001f)
					continue;

				const float long_edge_x_diff = long_segment.v1.x - long_segment.v0.x;
				const float short_edge_x_diff = short_edge.v1.x - short_edge.v0.x;

				float long_interp_factor = (short_edge.v0.y - long_segment.v0.y) / long_edge_y_diff;
				const float long_interp_step = 1.f / long_edge_y_diff;
				float short_interp_factor = .0f;
				const float short_interp_step = 1.f / short_edge_y_diff;

				// For each row the triangle is covering
				const uint32_t starting_row = static_cast<uint32_t>(long_segment.v0.y + .5f);
				const uint32_t ending_row = std::min(static_cast<uint32_t>(long_segment.v1.y + .5f), height);
				for (uint32_t row = starting_row; row < ending_row; ++row)
				{
					const float x0 = short_edge.v0.x + (short_edge_x_diff * short_interp_factor);
					const float x1 = long_segment.v0.x + (long_edge_x_diff * long_interp_factor);

					span_t span{ static_cast<uint32_t>(x0 + .5f), std::min(static_cast<uint32_t>(x1 + .5f), width) };

					// If the span is degenerate there is nothing to do
					uint32_t span_x_diff = span.x1 - span.x0;
					if (span_x_diff != 0)
					{
						// Save the current triangle index in the current pixel position
						for (uint32_t column = span.x0; column < span.x1; ++column)
							index_map[row * width + column] = tris_index;
					}

					short_interp_factor += short_interp_step;
					long_interp_factor += long_interp_step;
				}
			}
		}

		// Now downsample the result into the definitive indices map
		indices_map = new uint32_t[occlusion_map_width * occlusion_map_height];

		for (uint32_t i = 0; i < occlusion_map_height; ++i)
		{
			for (uint32_t j = 0; j < occlusion_map_width; ++j)
			{
				uint32_t samples[ssaa_scale * ssaa_scale] = {};

				// Collect all samples associated with this pixel
				for (uint32_t i_sample = 0; i_sample < ssaa_scale; ++i_sample)
				{
					for (uint32_t j_sample = 0; j_sample < ssaa_scale; ++j_sample)
					{
						const uint32_t dst_index = i_sample * ssaa_scale + j_sample;
						const uint32_t src_index = (i * ssaa_scale + i_sample) * width + j * ssaa_scale + j_sample;
						samples[dst_index] = index_map[src_index];
					}
				}

				uint32_t best_sample = std::numeric_limits<uint32_t>::max();
				ptrdiff_t sample_count = 0;
				uint32_t valid_sample = std::numeric_limits<uint32_t>::max();

				// Look for the samples with the higher count
				for (uint32_t sample_idx = 0; sample_idx < ssaa_scale * ssaa_scale; ++sample_idx)
				{
					ptrdiff_t count = std::count(std::begin(samples), std::end(samples), samples[sample_idx]);
					if (count > sample_count)
					{
						sample_count = count;
						best_sample = samples[sample_idx];
					}

					// Saving a valid sample for later, in case we don't find a sample which dominates
					if (samples[sample_idx] != std::numeric_limits<uint32_t>::max())
						valid_sample = samples[sample_idx];
				}

				// We may have an even distribution of samples and choosen an invalid one in the previous loop
				// If there was a valid sample we use it, if not then all samples are invalid so is safe to use it
				if (best_sample == std::numeric_limits<uint32_t>::max())
					best_sample = valid_sample;

				indices_map[i * occlusion_map_width + j] = best_sample;
			}
		}

		delete[] index_map;
	}
	
	// Step 2: Rasterize the occlusion map
	float* occlusion_map;
	{
		const tinyobj::mesh_t& mesh = shapes[mesh_index].mesh;

		occlusion_map = new float[occlusion_map_width * occlusion_map_height];
		memset(occlusion_map, 0, occlusion_map_width * occlusion_map_height * sizeof(float));

		const uint32_t quality = 8;
		const uint32_t sample_per_pixel = quality * 8;
		const float far_distance = 15.f;
		const float quadratic_attenuation = 1.0f;
		const float linear_attenuation = 1.1f;
		const bool smooth_interp_normal = true;

		// Calculate occlusion for each pixel in the occlusion map
		for (uint32_t i = 0; i < occlusion_map_height; ++i)
		{
			for (uint32_t j = 0; j < occlusion_map_width; ++j)
			{
				const uint32_t tris_index = indices_map[i * occlusion_map_width + j];

				// Check if a triangle actually cover this pixel
				if (tris_index == std::numeric_limits<uint32_t>::max())
					continue;

				// Calculate UV coordinates for the center of the current pixel
				const glm::vec2 p_coord{ j / static_cast<float>(occlusion_map_width),
										 i / static_cast<float>(occlusion_map_height) };

				const uint32_t v0_index = mesh.indices[tris_index * 3 + 0];
				const uint32_t v1_index = mesh.indices[tris_index * 3 + 1];
				const uint32_t v2_index = mesh.indices[tris_index * 3 + 2];

				// Get UV coordinates for the vertices of the triangle which includes this pixel
				const glm::vec2 v0_coord{ mesh.texcoords[v0_index * 2 + 0],
										  mesh.texcoords[v0_index * 2 + 1] };
				const glm::vec2 v1_coord{ mesh.texcoords[v1_index * 2 + 0],
										  mesh.texcoords[v1_index * 2 + 1] };
				const glm::vec2 v2_coord{ mesh.texcoords[v2_index * 2 + 0] ,
										  mesh.texcoords[v2_index * 2 + 1] };

				// Use baricentric interpolation to obtain the position and normal of the given pixel when projected on the mesh
				// http://answers.unity3d.com/questions/383804/calculate-uv-coordinates-of-3d-point-on-plane-of-m.html
				const glm::vec2 p0_coord = v0_coord - p_coord;
				const glm::vec2 p1_coord = v1_coord - p_coord;
				const glm::vec2 p2_coord = v2_coord - p_coord;

				const float area_tris = glm::length(glm::cross(glm::vec3(p0_coord - p1_coord, .0f), glm::vec3(p0_coord - p2_coord, .0f)));
				const float area0 = glm::length(glm::cross(glm::vec3(p1_coord, .0f), glm::vec3(p2_coord, .0f))) / area_tris;
				const float area1 = glm::length(glm::cross(glm::vec3(p2_coord, .0f), glm::vec3(p0_coord, .0f))) / area_tris;
				const float area2 = glm::length(glm::cross(glm::vec3(p0_coord, .0f), glm::vec3(p1_coord, .0f))) / area_tris;

				const glm::vec3 v0{ mesh.positions[v0_index * 3 + 0],
									mesh.positions[v0_index * 3 + 1],
									mesh.positions[v0_index * 3 + 2] };

				const glm::vec3 v1{ mesh.positions[v1_index * 3 + 0],
									mesh.positions[v1_index * 3 + 1],
									mesh.positions[v1_index * 3 + 2] };

				const glm::vec3 v2{ mesh.positions[v2_index * 3 + 0],
									mesh.positions[v2_index * 3 + 1],
									mesh.positions[v2_index * 3 + 2] };

				const glm::vec3 p = v0 * area0 + v1 * area1 + v2 * area2;

				const glm::vec3 n0{ mesh.normals[v0_index * 3 + 0],
									mesh.normals[v0_index * 3 + 1],
									mesh.normals[v0_index * 3 + 2] };

				const glm::vec3 n1{ mesh.normals[v1_index * 3 + 0],
									mesh.normals[v1_index * 3 + 1],
									mesh.normals[v1_index * 3 + 2] };

				const glm::vec3 n2{ mesh.normals[v2_index * 3 + 0],
									mesh.normals[v2_index * 3 + 1],
									mesh.normals[v2_index * 3 + 2] };

				// Setting smooth_inter_normal to false will just take the mean value of the normals
				glm::vec3 n;
				if (smooth_interp_normal)
				n = n0 * area0 + n1 * area1 + n2 * area2;
				else
				n = (n0 + n1 + n2) / 3.f;

				// Generate the rays in groups of 8, to make use of Embree AVX2 capabilities
				uint32_t num_hit = 0;
				float occlusion = .0f;
				for (uint32_t q = 0; q < quality; ++q)
				{
					RTCRay8 ray8{};
					for (uint32_t ray_id = 0; ray_id < 8; ++ray_id)
					{
						ray8.orgx[ray_id] = p.x;
						ray8.orgy[ray_id] = p.y;
						ray8.orgz[ray_id] = p.z;

						const glm::vec3 dir = cosine_weighted_hemisphere(n);
						ray8.dirx[ray_id] = dir.x;
						ray8.diry[ray_id] = dir.y;
						ray8.dirz[ray_id] = dir.z;

						ray8.tnear[ray_id] = .0001f;
						ray8.tfar[ray_id] = far_distance;

						ray8.geomID[ray_id] = RTC_INVALID_GEOMETRY_ID;
					}

					rtcIntersect8(&_ray_mask, embree_scene, ray8);

					// Sum up occlusion for each hit accounting for attenuation
					for (uint32_t ray_id = 0; ray_id < 8; ++ray_id)
					{
						if (ray8.geomID[ray_id] != RTC_INVALID_GEOMETRY_ID)
						{
							occlusion += 1.f - saturate(ray8.tfar[ray_id] / far_distance);
							++num_hit;
						}
					}
				}

				if (num_hit > 0)
				{
					occlusion /= sample_per_pixel;
					occlusion /= linear_attenuation;
					occlusion = std::pow(occlusion, quadratic_attenuation);
					occlusion_map[i * occlusion_map_width + j] = saturate(occlusion);
				}
			}
		}
	}

	/* Dithering
	const uint32_t dither_matrix[8][8] =
	{
		{ 0, 32, 8, 40, 2, 34, 10, 42 },
		{ 48, 16, 56, 24, 50, 18, 58, 26 },
		{ 12, 44, 4, 36, 14, 46, 6, 38 },
		{ 60, 28, 52, 20, 62, 30, 54, 22 },
		{ 3, 35, 11, 43, 1, 33, 9, 41 },
		{ 51, 19, 59, 27, 49, 17, 57, 25 },
		{ 15, 47, 7, 39, 13, 45, 5, 37 },
		{ 63, 31, 55, 23, 61, 29, 53, 21 }
	};

	// Step 3: Postprocess the occlusion map
	{
		for (uint32_t i = 0; i < occlusion_map_height; ++i)
		{
			for (uint32_t j = 0; j < occlusion_map_width; ++j)
			{
				uint32_t index = i * occlusion_map_width + j;
				
				uint32_t x = j % 8;
				uint32_t y = i % 8;

				float limit = static_cast<float>(dither_matrix[x][y] + 1) / 64.f;

				float value = occlusion_map[index];
				if (value < limit)
					occlusion_map[index] = saturate(value + (limit / 16.f));
				else
					occlusion_map[index] = saturate(value - (limit / 16.f));
			}
		}
	}
	*/

	/* Gaussian noise
	// Step 3: Postprocess the occlusion map
	{
		for (uint32_t i = 0; i < occlusion_map_height; ++i)
		{
			for (uint32_t j = 0; j < occlusion_map_width; ++j)
			{
				uint32_t index = i * occlusion_map_width + j;
				float value = occlusion_map[index];

				occlusion_map[index] = (value < .0001f) ? value : saturate(value + static_cast<float>(normal_distribution()));
			}
		}
	}
	*/

	// Step 4: Save the occlusion map to the disk
	{
		const elk::path out_path("maps/occlusion.dds");

	}

	bool should_run = true;
	while (should_run)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSdlGL3_ProcessEvent(&event);
			switch (event.type)
			{
				case SDL_QUIT:
					should_run = false;
					break;

				case SDL_KEYDOWN:
					if (event.key.keysym.sym == SDLK_ESCAPE)
						should_run = false;
					break;
			}
		}
		ImGui_ImplSdlGL3_NewFrame();

		{
			static float f = 0.0f;
			ImGui::Text("Hello, world!");
			ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		}

		// Update
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui::Render();
		// Render
		SDL_GL_SwapWindow(window);
	}

	ImGui_ImplSdlGL3_Shutdown();
	SDL_GL_DeleteContext(glcontext);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
