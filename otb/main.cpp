#include <iostream>
#include <cstdint>

#include "imgui/imgui.h"
#include "imgui_impl_sdl_gl3.hpp"
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

struct bbox
{
	float min_x;
	float max_x;
	float min_y;
	float max_y;
	float min_z;
	float max_z;
};

int main(int, char*[])
{
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
	_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);

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
										  RTC_SCENE_HIGH_QUALITY | RTC_SCENE_ROBUST, RTC_INTERSECT1);
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

	for (auto& s : shapes)
	{
		uint32_t vertex_occluded = 0;
		size_t num_vertices = s.mesh.positions.size() / 3;
		auto vertices = s.mesh.positions;
		auto normals = s.mesh.normals;

		for (uint32_t i = 0; i < num_vertices; ++i)
		{
			RTCRay ray{};
			ray.org[0] = vertices[i * 3 + 0];
			ray.org[1] = vertices[i * 3 + 1];
			ray.org[2] = vertices[i * 3 + 2];
			ray.dir[0] = normals[i * 3 + 0];
			ray.dir[1] = normals[i * 3 + 1];
			ray.dir[2] = normals[i * 3 + 2];

			ray.tnear = .0001f;
			ray.tfar = 100.f;

			ray.geomID = RTC_INVALID_GEOMETRY_ID;

			rtcOccluded(embree_scene, ray);

			if (ray.geomID != RTC_INVALID_GEOMETRY_ID)
				vertex_occluded++;
		}
		std::cout << "Shape " << s.name << " has " << vertex_occluded << " vertices occluded!" << std::endl;
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
