#include <iostream>
#include <cstdint>
#include <string>
#include <random>
#include <algorithm>
#include <limits>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <array>
#include <cmath>

#include "imgui/imgui.h"
#include "imgui_impl_sdl_gl3.hpp"
#include "glm/glm.hpp"
#include "GL/gl3w.h"
#include "SDL2/SDL.h"
#include "stb/stb_image_write.h"
#include "tinyobjloader/tiny_obj_loader.h"
#include "elektra/filesystem.hpp"
#define RMT_USE_OPENGL 1
#include "remotery/remotery.h"

#include "utils.hpp"
#include "image.hpp"
#include "embree.hpp"

#include "embree2/rtcore.h"
#pragma warning (push, 0)
#include "embree2/rtcore_ray.h"
#pragma warning (pop)
#include <xmmintrin.h>
#include <pmmintrin.h>

static constexpr uint32_t APP_WIDTH = 1280;
static constexpr uint32_t APP_HEIGHT = 720;
static constexpr char* APP_NAME = "Occlusion and Translucency Baker";

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

struct edge_t
{
	edge_t() = delete;
	edge_t(const glm::vec2& _v0, const glm::vec2& _v1) : v0(_v0), v1(_v1)
	{
		// Ordering vertices so that v0 is always the lower end
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
		// Ordering vertices so that x0 is always the left end
		if (_x0 > _x1)
		{
			x0 = _x1;
			x1 = _x0;
		}
	}

	uint32_t x0;
	uint32_t x1;
};

struct tile_work
{
	tile_work(uint32_t x, uint32_t y, bool v = true) : starting_x(x), starting_y(y), valid(v) {}

	uint32_t starting_x;
	uint32_t starting_y;
	bool valid;
};

#ifdef _DEBUG
static void gl_debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, void* userParam);
#endif

// TODO(Corralx): Set up some way to disable/enable remotery when needed
int main(int, char*[])
{
	// Setting the CPU register flags for Embree
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
	_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);

	ray_mask _ray_mask;
	for (uint32_t& ui : _ray_mask._)
		ui = 0xFFFFFFFF;

	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		std::cout << "SDL initialization error: " << SDL_GetError() << std::endl;
		return 1;
	}

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);

#ifdef _DEBUG
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG |
						SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#else
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif

	SDL_DisplayMode current;
	SDL_GetCurrentDisplayMode(0, &current);
	SDL_Window *window = SDL_CreateWindow(APP_NAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
										  APP_WIDTH, APP_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
	SDL_GLContext glcontext = SDL_GL_CreateContext(window);

	gl3wInit();

#ifdef _DEBUG
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback((GLDEBUGPROC)gl_debug_callback, nullptr);
#endif

	ImGui_ImplSdlGL3_Init(window);

	glViewport(0, 0, APP_WIDTH, APP_HEIGHT);
	glClearColor(1.f, .0f, .0f, 1.f);
	SDL_GL_SetSwapInterval(0);

	rmtSettings* settings = rmt_Settings();
	settings->input_handler_context = nullptr;
	settings->input_handler = [](const char* text, void*)
	{
		std::cout << "Remotery: " << text << std::endl;
	};

	Remotery* rmt;
	rmt_CreateGlobalInstance(&rmt);
	rmt_BindOpenGL();

	std::vector<tinyobj::shape_t> shapes;
	{
		elk::path mesh_path("chair/chair.obj");
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
			size_t num_tris = shapes[i].mesh.indices.size() / 3;

			assert(num_vertices == num_normals && num_vertices == num_tex_coords);
			
			std::cout << std::endl << "Shape " << i << " (" << shapes[i].name << ")" << std::endl;
			std::cout << "Vertices: " << num_vertices << std::endl;
			std::cout << "Normals: " << num_normals << std::endl;
			std::cout << "Texture coords: " << num_tex_coords << std::endl;
			std::cout << "Triangles: " << num_tris << std::endl;
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

	const uint32_t mesh_index = 0;
	const uint32_t occlusion_map_width = 1024;
	const uint32_t occlusion_map_height = occlusion_map_width;

	/*
	// Step 1: Find the triangle index covering each pixel in the occlusion map
	uint32_t* indices_map;
	{
		const uint8_t ssaa_scale = 2;
		const uint32_t width = occlusion_map_width * ssaa_scale;
		const uint32_t height = occlusion_map_height * ssaa_scale;

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

				// We may have an even distribution of samples and choose an invalid one in the previous loop
				// If there was a valid sample we use it, if not then all samples are invalid and is safe to use it
				if (best_sample == std::numeric_limits<uint32_t>::max())
					best_sample = valid_sample;

				indices_map[i * occlusion_map_width + j] = best_sample;
			}
		}

		delete[] index_map;
	}
	*/

	// EXPERIMENTAL(Corralx): Rasterize uvs with the gpu
	uint32_t* indices_map;
	{
		indices_map = new uint32_t[occlusion_map_width * occlusion_map_height];
		uint32_t* output = indices_map;
		memset(output, 255, occlusion_map_width * occlusion_map_height * sizeof(uint32_t));

		// Texture
		uint32_t texture;
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, occlusion_map_width, occlusion_map_height, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, output);

		// Framebuffer
		uint32_t framebuffer;
		glGenFramebuffers(1, &framebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

		assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

		// VAO
		uint32_t vao;
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		// Vertices
		const auto& vertex_data = shapes[mesh_index].mesh.texcoords;
		uint32_t vertex_buffer;
		glGenBuffers(1, &vertex_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * vertex_data.size(), vertex_data.data(), GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

		// Indices
		const auto& index_data = shapes[mesh_index].mesh.indices;
		uint32_t index_buffer;
		glGenBuffers(1, &index_buffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * index_data.size(), index_data.data(), GL_STATIC_DRAW);

		// Program
		const char* vs_source =
			"#version 330 core\n \
			 \n \
			 layout(location = 0) in vec2 position;\n \
			 \n \
			 void main()\n \
			 {\n \
				 gl_Position = vec4(position * 2.0 - 1.0, 0.5, 1.0);\n \
			 }\n \
			";

		const char* fs_source =
			"#version 330 core\n \
			 \n \
			 layout(location = 0) out uint out_color;\n \
			 \n \
			 void main()\n \
			 {\n \
				 out_color = uint(gl_PrimitiveID);\n \
			 }\n \
			";
		fs_source;

		uint32_t vs = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vs, 1, &vs_source, nullptr);
		glCompileShader(vs);

		uint32_t fs = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fs, 1, &fs_source, nullptr);
		glCompileShader(fs);

		uint32_t program = glCreateProgram();
		glAttachShader(program, vs);
		glAttachShader(program, fs);
		glLinkProgram(program);

		assert(glGetError() == GL_NO_ERROR);

		glUseProgram(program);
		glViewport(0, 0, occlusion_map_width, occlusion_map_height);

		// Draw
		glDrawElements(GL_TRIANGLES, (uint32_t)index_data.size(), GL_UNSIGNED_INT, nullptr);

		// Copy back the data
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, output);

		// TODO: Save to disk the output
		uint8_t* file = new uint8_t[occlusion_map_width * occlusion_map_height];
		memset(file, 255, occlusion_map_width * occlusion_map_height);

		for (uint32_t i = 0; i < occlusion_map_height; ++i)
		{
			for (uint32_t j = 0; j < occlusion_map_width; ++j)
			{
				uint32_t index = i * occlusion_map_width + j;
				uint32_t value = output[index];

				if (value != std::numeric_limits<uint32_t>::max())
					file[index] = 0;
			}
		}

		stbi_write_tga("maps/raster.tga", occlusion_map_width, occlusion_map_height, 1, file);

		glViewport(0, 0, APP_WIDTH, APP_HEIGHT);
		glUseProgram(0);
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// Free memory
		delete[] file;

		// Delete program
		glDeleteShader(vs);
		glDeleteShader(fs);
		glDeleteProgram(program);

		// Delete buffers
		glDeleteBuffers(1, &index_buffer);
		glDeleteBuffers(1, &vertex_buffer);
		glDeleteVertexArrays(1, &vao);

		// Delete framebuffer
		glDeleteTextures(1, &texture);
		glDeleteFramebuffers(1, &framebuffer);
		assert(glGetError() == GL_NO_ERROR);
	}

	// Step 2: Generate the occlusion map
	float* occlusion_map;
	{
		const tinyobj::mesh_t& mesh = shapes[mesh_index].mesh;

		occlusion_map = new float[occlusion_map_width * occlusion_map_height];
		memset(occlusion_map, 0, occlusion_map_width * occlusion_map_height * sizeof(float));

		const uint32_t quality = 1;
		const uint32_t sample_per_pixel = quality * 8;
		const float far_distance = 15.f;
		const float quadratic_attenuation = 1.0f;
		const float linear_attenuation = 1.1f;
		const bool smooth_interp_normal = true;

		const uint32_t tile_width = 64;
		const uint32_t tile_height = tile_width;
		const uint32_t num_tile_width = occlusion_map_width / tile_width;
		const uint32_t num_tile_height = occlusion_map_height / tile_height;

		std::vector<std::thread> workers;
		std::queue<tile_work> work_queue;
		std::mutex work_mutex;
		const uint32_t expected_num_workers = 8;
		const uint32_t num_workers = std::min(std::thread::hardware_concurrency(), expected_num_workers);

		auto get_work = [&work_queue, &work_mutex]() -> tile_work
		{
			std::lock_guard<std::mutex> lock(work_mutex);

			if (work_queue.empty())
				return { std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max(), false };

			auto work = work_queue.front();
			work_queue.pop();
			return work;
		};

		auto do_work = [&get_work, indices_map, &mesh, far_distance, linear_attenuation, quadratic_attenuation,
						occlusion_map, &_ray_mask, embree_scene, tile_height, tile_width, quality, smooth_interp_normal,
						sample_per_pixel, occlusion_map_height, occlusion_map_width]()
		{
			while (true)
			{
				auto work = get_work();
				if (!work.valid)
					return;

				for (uint32_t i = work.starting_y; i < work.starting_y + tile_height; ++i)
				{
					for (uint32_t j = work.starting_x; j < work.starting_x + tile_width; ++j)
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

								const glm::vec3 dir = cosine_weighted_hemisphere_sample(n);
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
		};

		for (uint32_t i = 0; i < num_tile_height; ++i)
			for (uint32_t j = 0; j < num_tile_width; ++j)
				work_queue.emplace(j * tile_width, i * tile_height);

		for (uint32_t w = 0; w < num_workers; ++w)
			workers.push_back(std::thread(do_work));

		for (auto& w : workers)
			w.join();
	}

	// Step 3: Post process
	{
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
		*/

		// Gaussian blur
		const uint32_t kernel_size = 3;
		const std::array<float, kernel_size> gaussian_kernel = generate_gaussian_kernel_1d<kernel_size>(1.f);
		uint32_t num_blur_pass = 3;

		const int32_t kernel_half_width = static_cast<int32_t>(kernel_size) / 2;

		float* temp_buffer = nullptr;
		if (num_blur_pass > 0)
			temp_buffer = new float[occlusion_map_width * occlusion_map_height];

		for (uint32_t pass = 0; pass < num_blur_pass; ++pass)
		{
			// First pass: blur horizontally
			for (uint32_t i = 0; i < occlusion_map_height; ++i)
			{
				for (uint32_t j = 0; j < occlusion_map_width; ++j)
				{
					float sum = .0f;
					for (int32_t k = -kernel_half_width; k <= kernel_half_width; ++k)
					{
						const uint32_t sample_j = clamp(j + k, (uint32_t)0, occlusion_map_width - 1);
						const uint32_t index = i * occlusion_map_width + sample_j;
						sum += occlusion_map[index] * gaussian_kernel[k + kernel_half_width];
					}

					const uint32_t index = i * occlusion_map_width + j;
					temp_buffer[index] = sum;
				}
			}

			// Second pass: blur vertically
			for (uint32_t j = 0; j < occlusion_map_width; ++j)
			{
				for (uint32_t i = 0; i < occlusion_map_height; ++i)
				{
					float sum = .0f;
					for (int32_t k = -kernel_half_width; k <= kernel_half_width; ++k)
					{
						const uint32_t sample_i = clamp(i + k, (uint32_t)0, occlusion_map_height - 1);
						const uint32_t index = sample_i * occlusion_map_width + j;
						sum += temp_buffer[index] * gaussian_kernel[k + kernel_half_width];
					}

					const uint32_t index = i * occlusion_map_width + j;
					occlusion_map[index] = sum;
				}
			}
		}

		if (temp_buffer)
			delete[] temp_buffer;

		// Invert the values
		for (uint32_t i = 0; i < occlusion_map_height; ++i)
		{
			for (uint32_t j = 0; j < occlusion_map_width; ++j)
			{
				uint32_t index = i * occlusion_map_width + j;
				float value = occlusion_map[index];

				occlusion_map[index] = saturate(1.f - value);
			}
		}
	}

	// Step 4: Save the occlusion map to the disk
	{
		elk::path base_path("maps");
		base_path /= "occlusion_";
		base_path += shapes[mesh_index].name;
		const elk::path out_path_tga(elk::path(base_path).concat(".tga"));
		const elk::path out_path_hdr(elk::path(base_path).concat(".hdr"));
		const elk::path out_path_png(elk::path(base_path).concat(".png"));

		uint8_t* out_image = new uint8_t[occlusion_map_width * occlusion_map_height];
		memset(out_image, 0, occlusion_map_width * occlusion_map_height);

		for (uint32_t i = 0; i < occlusion_map_height; ++i)
		{
			for (uint32_t j = 0; j < occlusion_map_width; ++j)
			{
				uint32_t index = i * occlusion_map_width + j;
				float value = occlusion_map[index];

				uint8_t quantized = static_cast<uint8_t>(value * 255);
				out_image[index] = std::min(quantized, (uint8_t)255);
			}
		}
		
		stbi_write_tga(out_path_tga.c_str(), occlusion_map_width, occlusion_map_height, 1, out_image);
		stbi_write_hdr(out_path_hdr.c_str(), occlusion_map_width, occlusion_map_height, 1, occlusion_map);
		stbi_write_png(out_path_png.c_str(), occlusion_map_width, occlusion_map_height, 1, out_image, 0);

		delete[] out_image;
	}

	bool should_run = true;
	while (should_run)
	{
		rmt_BeginCPUSample(RenderLoop);

		rmt_BeginCPUSample(EventManagement);
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
		rmt_EndCPUSample();

		// TODO: Update

		rmt_BeginCPUSample(BufferClear);
		glClear(GL_COLOR_BUFFER_BIT);
		rmt_EndCPUSample();

		// TODO: Render

		rmt_BeginCPUSample(ImGuiRender);
		ImGui_ImplSdlGL3_NewFrame();
		{
			static float f = 0.0f;
			ImGui::Text("Hello, world!");
			ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		}
		ImGui::Render();
		rmt_EndCPUSample();

		SDL_GL_SwapWindow(window);
		rmt_EndCPUSample();
	}

	rmt_UnbindOpenGL();
	rmt_DestroyGlobalInstance(rmt);

	ImGui_ImplSdlGL3_Shutdown();
	SDL_GL_DeleteContext(glcontext);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}

#ifdef _DEBUG
void gl_debug_callback(GLenum, GLenum type, GLuint id, GLenum severity, GLsizei, const GLchar* message, void*)
{
	using namespace std;

	cout << "---------------------opengl-callback-start------------" << endl;
	cout << "message: " << message << endl;
	cout << "type: ";
	switch (type)
	{
		case GL_DEBUG_TYPE_ERROR:
			cout << "ERROR";
			break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
			cout << "DEPRECATED_BEHAVIOR";
			break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
			cout << "UNDEFINED_BEHAVIOR";
			break;
		case GL_DEBUG_TYPE_PORTABILITY:
			cout << "PORTABILITY";
			break;
		case GL_DEBUG_TYPE_PERFORMANCE:
			cout << "PERFORMANCE";
			break;
		case GL_DEBUG_TYPE_OTHER:
			cout << "OTHER";
			break;
	}
	cout << endl;

	cout << "id: " << id << endl;
	cout << "severity: ";
	switch (severity)
	{
		case GL_DEBUG_SEVERITY_LOW:
			cout << "LOW";
			break;
		case GL_DEBUG_SEVERITY_MEDIUM:
			cout << "MEDIUM";
			break;
		case GL_DEBUG_SEVERITY_HIGH:
			cout << "HIGH";
			break;
	}
	cout << endl;
	cout << "---------------------opengl-callback-end--------------" << endl;
}
#endif
