#include "rasterizer.hpp"
#include "mesh.hpp"
#include "utils.hpp"

#include "GL/gl3w.h"
#include "SDL2/SDL.h"
#include "glm/glm.hpp"

// TODO(Corralx): Implement the functions

using image_u32 = image<image_format::U32>;

static void rasterize_software_helper(const mesh_t& mesh, image_u32& image, uint8_t supersampling, std::promise<void> promise)
{
	mesh;
	image;
	supersampling;

	// TODO(Corralx)

	promise.set_value_at_thread_exit();
}

/*
std::future<void> rasterize_triangle_software(const mesh_t & mesh, image_u32& image, uint8_t supersampling)
{
	std::promise<void> promise;
	std::future<void> future;
	std::thread([](const mesh_t& mesh, image_u32& image, uint8_t supersampling, std::promise<void> promise)
	{ rasterize_software_helper(mesh, image, supersampling, std::move(promise)); },
				std::ref(mesh), std::ref(image), supersampling, std::move(promise)).detach();

	return std::move(future);
}
*/

std::future<void> rasterize_triangle_software(const mesh_t & mesh, image_u32& image, uint8_t supersampling)
{
	return async_apply(rasterize_software_helper, std::ref(mesh), std::ref(image), supersampling);
}

extern SDL_Window* window;

static void rasterize_hardware_helper(const mesh_t& mesh, image_u32& image, std::promise<void> promise)
{
	static auto context = SDL_GL_CreateContext(window);
	SDL_GL_MakeCurrent(window, context);

	// Texture
	uint32_t texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, image.width(), image.height(), 0, GL_RED_INTEGER, GL_UNSIGNED_INT, image.raw());

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
	const auto& vertex_data = mesh.texture_coords();
	uint32_t vertex_buffer;
	glGenBuffers(1, &vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, 2 * sizeof(float) * vertex_data.size(), vertex_data.data(), GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

	// Indices
	const auto& index_data = mesh.faces();
	uint32_t index_buffer;
	glGenBuffers(1, &index_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 3 * sizeof(uint32_t) * index_data.size(), index_data.data(), GL_STATIC_DRAW);

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
	glViewport(0, 0, image.width(), image.height());

	// Draw
	glDrawElements(GL_TRIANGLES, (uint32_t)index_data.size() * 3, GL_UNSIGNED_INT, nullptr);

	// Copy back the data
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, image.raw());

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

	promise.set_value_at_thread_exit();
}

/*
std::future<void> rasterize_triangle_hardware(const mesh_t& mesh, image_u32& image)
{
	std::promise<void> promise;
	std::future<void> future;
	std::thread([](const mesh_t& mesh, image_u32& image, std::promise<void> promise)
				{ rasterize_hardware_helper(mesh, image, std::move(promise)); },
				std::ref(mesh), std::ref(image), std::move(promise)).detach();

	return std::move(future);
}
*/

std::future<void> rasterize_triangle_hardware(const mesh_t& mesh, image_u32& image)
{
	return async_apply(rasterize_hardware_helper, std::ref(mesh), std::ref(image));
}
