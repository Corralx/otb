#include <iostream>
#include <cstdint>
#include <random>
#include <algorithm>

#include "imgui/imgui.h"
#include "imgui_impl_sdl_gl3.hpp"
#include "glm/glm.hpp"
#include "GL/gl3w.h"
#include "SDL2/SDL.h"
#include "tinyobjloader/tiny_obj_loader.h"
#include "elektra/filesystem.hpp"

extern "C"
{
#include "targa.h"
}

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

struct bbox
{
	float min_x;
	float max_x;
	float min_y;
	float max_y;
	float min_z;
	float max_z;
};

double random_float()
{
	static std::random_device rd;
	static std::mt19937 gen(rd());
	static std::uniform_real_distribution<> dis(.0f, 1.f);

	return dis(gen);
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

struct RTCORE_ALIGN(16) ray_mask
{
	uint32_t _[16];
};

bool point_in_tris(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b, const glm::vec2& c)
{
	if (same_side(p, a, b, c) && same_side(p, b, a, c) && same_side(p, c, a, b))
		return true;
	else
		return false;
}

int main(int, char*[])
{
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
	bbox box{};
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
			
			auto vertices = shapes[i].mesh.positions;

			for (uint32_t j = 0; j < num_vertices; ++j)
			{
				float x = vertices[j * 3 + 0];
				float y = vertices[j * 3 + 1];
				float z = vertices[j * 3 + 2];

				box.max_x = box.max_x > x ? box.max_x : x;
				box.min_x = box.min_x < x ? box.min_x : x;

				box.max_y = box.max_y > y ? box.max_y : y;
				box.min_y = box.min_y < y ? box.min_y : y;

				box.max_z = box.max_z > z ? box.max_z : z;
				box.min_z = box.min_z < z ? box.min_z : z;
			}
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

	{
		auto& plane = shapes[0].mesh;
		elk::path path("maps/plane.tga");

		const uint32_t width = 1024;
		const uint32_t height = width;
		uint8_t* maps = new uint8_t[width * height];

		for (uint32_t i = 0; i < height; ++i)
		{
			for (uint32_t j = 0; j < width; ++j)
			{
				maps[i * width + j] = 255;

				glm::vec2 p_coord{};
				p_coord.x = j / static_cast<float>(width);
				p_coord.y = i / static_cast<float>(height);

				glm::vec3 p{};

				// Look up coordinates
				for (uint32_t tris = 0; tris < plane.indices.size(); tris += 3)
				{
					auto v0_index = plane.indices[tris + 0];
					auto v1_index = plane.indices[tris + 1];
					auto v2_index = plane.indices[tris + 2];

					glm::vec2 v0_coord{}, v1_coord{}, v2_coord{};

					v0_coord.x = plane.texcoords[v0_index * 2 + 0];
					v0_coord.y = plane.texcoords[v0_index * 2 + 1];
					v1_coord.x = plane.texcoords[v1_index * 2 + 0];
					v1_coord.y = plane.texcoords[v1_index * 2 + 1];
					v2_coord.x = plane.texcoords[v2_index * 2 + 0];
					v2_coord.y = plane.texcoords[v2_index * 2 + 1];

					if (!point_in_tris(p_coord, v0_coord, v1_coord, v2_coord))
						continue;

					// http://answers.unity3d.com/questions/383804/calculate-uv-coordinates-of-3d-point-on-plane-of-m.html
					glm::vec3 v0{}, v1{}, v2{};
					v0.x = plane.positions[v0_index * 3 + 0];
					v0.y = plane.positions[v0_index * 3 + 1];
					v0.z = plane.positions[v0_index * 3 + 2];
					v1.x = plane.positions[v1_index * 3 + 0];
					v1.y = plane.positions[v1_index * 3 + 1];
					v1.z = plane.positions[v1_index * 3 + 2];
					v2.x = plane.positions[v2_index * 3 + 0];
					v2.y = plane.positions[v2_index * 3 + 1];
					v2.z = plane.positions[v2_index * 3 + 2];

					auto p0_coord = v0_coord - p_coord;
					auto p1_coord = v1_coord - p_coord;
					auto p2_coord = v2_coord - p_coord;

					auto area_tris = glm::length(glm::cross(glm::vec3(p0_coord - p1_coord, .0f), glm::vec3(p0_coord - p2_coord, .0f)));
					auto area0 = glm::length(glm::cross(glm::vec3(p1_coord, .0f), glm::vec3(p2_coord, .0f))) / area_tris;
					auto area1 = glm::length(glm::cross(glm::vec3(p2_coord, .0f), glm::vec3(p0_coord, .0f))) / area_tris;
					auto area2 = glm::length(glm::cross(glm::vec3(p0_coord, .0f), glm::vec3(p1_coord, .0f))) / area_tris;

					p = v0 * area0 + v1 * area1 + v2 * area2;
					
					RTCRay8 ray8{};
					for (uint32_t ray_id = 0; ray_id < 8; ++ray_id)
					{
						ray8.orgx[ray_id] = p.x;
						ray8.orgy[ray_id] = p.y;
						ray8.orgz[ray_id] = p.z;

						glm::vec3 dir = cosine_weighted_hemisphere({ .0f, 1.f, .0f });
						ray8.dirx[ray_id] = dir.x;
						ray8.diry[ray_id] = dir.y;
						ray8.dirz[ray_id] = dir.z;

						ray8.tnear[ray_id] = .0001f;
						ray8.tfar[ray_id] = 20.f;

						ray8.geomID[ray_id] = RTC_INVALID_GEOMETRY_ID;
					}

					rtcIntersect8(&_ray_mask, embree_scene, ray8);

					uint32_t num_hit = 0;
					for (uint32_t ray_id = 0; ray_id < 8; ++ray_id)
					{
						if (ray8.geomID[ray_id] != RTC_INVALID_GEOMETRY_ID)
							++num_hit;
					}

					// TODO: Better mapping
					maps[i * width + j] = (num_hit > 0) ? (static_cast<uint8_t>(255 / 8 * (8 - num_hit))) : 255;

					/*
					RTCRay ray{};
					ray.org[0] = p.x;
					ray.org[1] = p.y;
					ray.org[2] = p.z;
					ray.dir[0] = .0f;
					ray.dir[1] = 1.f;
					ray.dir[2] = .0f;

					ray.tnear = .0001f;
					ray.tfar = 100.f;

					ray.geomID = RTC_INVALID_GEOMETRY_ID;

					rtcOccluded(embree_scene, ray);

					if (ray.geomID != RTC_INVALID_GEOMETRY_ID)
						maps[i * width + j] = 0;
					*/

					break;
				}
			}
		}

		auto res = tga_write_mono(path.c_str(), maps, width, height);
		if (res != TGA_NOERR)
			std::cerr << "Error writing tga image!" << std::endl;

		delete[] maps;
	}

	bool should_run = true;
	while (should_run)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSdlGL3_ProcessEvent(&event);
			if (event.type == SDL_QUIT)
				should_run = false;
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
