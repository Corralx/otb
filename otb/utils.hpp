#pragma once

#include "mesh.hpp"
#include "image.hpp"

#include "elektra/optional.hpp"

#include <cstdint>
#include <future>
#include <array>
#include <vector>
#include <thread>

constexpr double PI = 3.14159265358979323846264338327950288;

namespace elk
{
class path;
}

// TODO(Corralx): Found a generic way to return an error
std::vector<mesh_t> load_meshes(const elk::path& path);

elk::optional<uint32_t> load_program(const elk::path& vs_path, const elk::path& fs_path);

enum class image_extension : uint8_t
{
	PNG = 0,
	BMP = 1,
	TGA = 2
};

// TODO(Corralx): Make async version of file saving? Could take quite a while
template<pixel_format F>
bool write_image(const elk::path& path, const image<F>& image, image_extension ext);

bool write_image(const elk::path& path, const image<pixel_format::F32>& image);

// TODO(Corralx): Functions to convert between formats (like R32 -> U8)

bool point_in_tris(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b, const glm::vec2& c);

glm::vec3 cosine_weighted_hemisphere_sample(glm::vec3 n);


std::vector<float> generate_gaussian_kernel_1d(float sigma, uint32_t kernel_size);

template<typename T>
T saturate(T value)
{
	return std::min(static_cast<T>(1), std::max(static_cast<T>(0), value));
}

template<typename T>
T clamp(T value, T min, T max)
{
	return std::min(max, std::max(min, value));
}

template<typename T>
bool is_ready(const std::future<T>& f)
{
	// NOTE(Corralx): This doesn't work for deferred future
	return f.wait_for(0) == std::future_status::ready;
}

template<typename Func, typename ...Args>
std::future<void> async_apply(Func f, Args&&... args)
{
	std::promise<void> promise;
	std::future<void> future = promise.get_future();
	std::thread(f, std::forward<Args>(args)..., std::move(promise)).detach();

	return future;
}
