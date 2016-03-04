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

static void rasterize_hardware_helper(const mesh_t& mesh, image_u32& image, std::promise<void> promise)
{
	mesh;
	image;

	// TODO(Corralx)

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
