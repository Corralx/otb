#include <iostream>
#include <chrono>

using hr_clock = std::chrono::high_resolution_clock;
using millis = std::chrono::milliseconds;

#include "imgui/imgui.h"
#include "imgui_sdl_bridge.hpp"
#include "GL/gl3w.h"
#include "SDL2/SDL.h"
#include "remotery/remotery.h"
#include "elektra/filesystem.hpp"
#include "elektra/machine_specs.hpp"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

/*
#include "chaiscript/chaiscript.hpp"
#include "chaiscript/chaiscript_stdlib.hpp"
*/

#include "utils.hpp"
#include "image.hpp"
#include "embree.hpp"
#include "mesh.hpp"
#include "occlusion.hpp"
#include "rasterizer.hpp"
#include "postprocess.hpp"
#include "configuration.hpp"
#include "buffer_manager.hpp"
#include "mesh_data_manager.hpp"

static constexpr uint32_t APP_WIDTH = 1280;
static constexpr uint32_t APP_HEIGHT = 720;
static constexpr char* APP_NAME = "Occlusion and Translucency Baker";
static constexpr uint32_t OPENGL_MAJOR_VERSION = 3;
static constexpr uint32_t OPENGL_MINOR_VERSION = 3;
static constexpr uint32_t MAP_SIZE = 256;
static constexpr char* CONFIG_FILENAME = "resources/config.json";

#ifdef _DEBUG
static void gl_debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, void* userParam);
#endif

SDL_Window* window;

// TODO(Corralx): Investigate an ImGui file dialog and a notification system
int main(int, char*[])
{
	/*
	chaiscript::ChaiScript chai(chaiscript::Std_Lib::library());
	chai.add(chaiscript::fun(&helloWorld),
			 "helloWorld");

	chai.eval("helloWorld(\"Bob\");");
	*/

	if (!init_configuration(CONFIG_FILENAME))
	{
		std::cerr << "Error loading the configuration from file!" << std::endl;
		return 1;
	}

	std::cout << "Selected shader path: " << global_config.shader_path << std::endl;
	std::cout << "Selected output path: " << global_config.output_path << std::endl;

	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		std::cerr << "SDL initialization error: " << SDL_GetError() << std::endl;
		return 1;
	}

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, OPENGL_MAJOR_VERSION);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, OPENGL_MINOR_VERSION);
	SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);

#ifdef _DEBUG
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG |
						SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#else
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif

	SDL_DisplayMode current;
	SDL_GetCurrentDisplayMode(0, &current);
	window = SDL_CreateWindow(APP_NAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
							  APP_WIDTH, APP_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
	SDL_GLContext glcontext = SDL_GL_CreateContext(window);

	gl3wInit();

#ifdef _DEBUG
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback((GLDEBUGPROC)gl_debug_callback, nullptr);
#endif

	imgui_init(window);

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_STENCIL_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glViewport(0, 0, APP_WIDTH, APP_HEIGHT);
	glClearColor(1.f, .0f, .0f, 1.f);
	SDL_GL_SetSwapInterval(0);

	auto base_program = load_program(global_config.shader_path / "base.vs", global_config.shader_path / "base.fs");
	if (!base_program)
	{
		std::cerr << "Error while loading the base program!" << std::endl;
		return 1;
	}
	uint32_t matrices_block_index = glGetUniformBlockIndex(base_program.value(), "Matrices");


	rmtSettings* settings = rmt_Settings();
	settings->input_handler_context = nullptr;
	settings->input_handler = [](const char* text, void*)
	{
		std::cout << "Remotery: " << text << std::endl;
	};

	Remotery* rmt;
	rmt_CreateGlobalInstance(&rmt);
	rmt_BindOpenGL();

	std::cout << "Loading meshes..." << std::endl;
	elk::path mesh_path("resources/meshes/armor.obj");
	auto shapes = load_meshes(mesh_path);
	if (shapes.empty())
	{
		std::cerr << "Error loading meshes!" << std::endl;
		return 1;
	}

	std::cout << "Shapes found: " << shapes.size() << std::endl;

	for (uint32_t i = 0; i < shapes.size(); ++i)
	{
		size_t num_vertices = shapes[i].vertices().size();
		size_t num_normals = shapes[i].normals().size();
		size_t num_tex_coords = shapes[i].texture_coords().size();
		size_t num_tris = shapes[i].faces().size();

		std::cout << std::endl << "Shape " << i << std::endl;
		std::cout << "Vertices: " << num_vertices << std::endl;
		std::cout << "Normals: " << num_normals << std::endl;
		std::cout << "Texture coords: " << num_tex_coords << std::endl;
		std::cout << "Triangles: " << num_tris << std::endl;
	}
	std::cout << std::endl;

	const uint32_t mesh_index = 0;

	buffer_manager buffer_mgr;
	mesh_data_manager mesh_data_mgr(buffer_mgr);
	{
		size_t vertices_index = buffer_mgr.create_buffer(buffer_type::VERTEX, buffer_usage::STATIC, shapes[mesh_index].vertices());
		size_t normals_index = buffer_mgr.create_buffer(buffer_type::VERTEX, buffer_usage::STATIC, shapes[mesh_index].normals());
		size_t tex_coords_index = buffer_mgr.create_buffer(buffer_type::VERTEX, buffer_usage::STATIC, shapes[mesh_index].texture_coords());
		size_t faces_index = buffer_mgr.create_buffer(buffer_type::INDEX, buffer_usage::STATIC, shapes[mesh_index].faces());

		mesh_data_mgr.bind_data(shapes[mesh_index], vertices_index, normals_index, tex_coords_index, faces_index);
	}

	std::cout << "Initializing Embree..." << std::endl;
	embree::context context;
	for (const mesh_t& m : shapes)
		context.add_mesh(m);
	if (!context.commit())
	{
		std::cerr << "Error initializing Embree!" << std::endl;
		return 1;
	}
	
	std::cout << "Rasterizing UVs..." << std::endl;
	image<pixel_format::U32> indices_map(MAP_SIZE, MAP_SIZE);
	indices_map.reset(255);
	rasterize_triangle_hardware(shapes[mesh_index], indices_map).get();
	std::cout << "Indices map occupies " << indices_map.memory() << " bytes!" << std::endl;

	std::cout << "Calculating occlusion map..." << std::endl;
	image<pixel_format::F32> occlusion_map(MAP_SIZE, MAP_SIZE);
	occlusion_map.reset(0);

	occlusion_params params{};
	params.min_distance = .0001f;
	params.max_distance = 5.f;
	params.smooth_normal_interpolation = true;
	params.linear_attenuation = 1.f;
	params.quadratic_attenuation = 1.f;
	params.tile_width = 64;
	params.tile_height = 64;
	params.quality = 1;
	params.worker_num = (uint8_t)elk::number_of_cores();

	auto start_time = hr_clock::now();
	generate_occlusion_map(context, shapes[mesh_index], params, indices_map, occlusion_map).get();
	auto end_time = hr_clock::now();
	std::cout << "Occlusion map occupies " << occlusion_map.memory() << " bytes!" << std::endl;
	std::cout << "Calculation has taken " << std::chrono::duration_cast<millis>(end_time - start_time).count() << " ms!" << std::endl;

	std::cout << "Postprocessing occlusion map..." << std::endl;
	gaussian_blur(occlusion_map, 1, 3, 1.f).get();
	invert(occlusion_map).get();

	std::cout << "Saving to disk..." << std::endl;
	write_image(global_config.output_path / "occlusion_map.hdr", occlusion_map);
	std::cout << "Done!" << std::endl;

	std::vector<glm::mat4> matrices(3);
	matrices[0] = glm::perspective(glm::radians(45.f), (float)APP_WIDTH / (float)APP_HEIGHT, 1.f, 1000.f);
	matrices[1] = glm::lookAt(glm::vec3(.0f, .0f, 10.f), glm::vec3(.0f, .0f, 0.f), glm::vec3(.0f, 1.f, .0f));
	matrices[2] = matrices[0] * matrices[1];

	const uint32_t matrices_binding_point = 0;
	size_t matrices_buffer = buffer_mgr.create_buffer(buffer_type::UNIFORM, buffer_usage::DYNAMIC, matrices);
	glBindBufferBase(GL_UNIFORM_BUFFER, matrices_binding_point, buffer_mgr[matrices_buffer]);
	glUniformBlockBinding(base_program.value(), matrices_block_index, matrices_binding_point);

	uint32_t occlusion_vao = mesh_data_mgr[shapes[mesh_index]].occlusion_vao;
	uint32_t occlusion_program = base_program.value();

	bool should_run = true;
	while (should_run)
	{
		rmt_BeginCPUSample(RenderLoop);

		rmt_BeginCPUSample(EventManagement);
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			imgui_process_event(&event);
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

		// Update

		rmt_BeginCPUSample(BufferClear);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		rmt_EndCPUSample();
		
		rmt_BeginCPUSample(SceneRender);
		glBindVertexArray(occlusion_vao);
		glUseProgram(occlusion_program);
		void* matrices_ptr = glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4) * matrices.size(),
											  GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
		memcpy(matrices_ptr, matrices.data(), sizeof(glm::mat4) * matrices.size());
		glUnmapBuffer(GL_UNIFORM_BUFFER);
		glDrawElements(GL_TRIANGLES, (uint32_t)shapes[mesh_index].faces().size() * 3, GL_UNSIGNED_INT, nullptr);
		rmt_EndCPUSample();
		assert(glGetError() == GL_NO_ERROR);

		rmt_BeginCPUSample(ImGuiRender);
		imgui_new_frame();
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

	imgui_shutdown();
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
