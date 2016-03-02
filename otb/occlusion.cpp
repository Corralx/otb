#include "occlusion.hpp"
#include "mesh.hpp"

#include "glm/glm.hpp"

#include <thread>
#include <mutex>
#include <queue>
#include <vector>

using image_f32 = image<image_format::F32>;

static void generate_occlusion_helper(const mesh_t& mesh, occlusion_params params, image_f32& image, std::promise<void> promise)
{
	mesh;
	params;
	image;

	// TODO(Corralx): Actually generate the occlusion map

	promise.set_value_at_thread_exit();
}

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