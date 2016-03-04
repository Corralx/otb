#pragma once

#include "mesh.hpp"
#include "image.hpp"

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

std::vector<mesh_t> load_mesh(const elk::path& path);

enum class image_extension : uint8_t
{
	PNG = 0,
	BMP = 1,
	TGA = 2
};

template<image_format F>
bool write_image(const elk::path& path, const image<F>& image, image_extension ext);

bool write_image(const elk::path& path, const image<image_format::F32>& image);

bool point_in_tris(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b, const glm::vec2& c);

glm::vec3 cosine_weighted_hemisphere_sample(glm::vec3 n);

// TODO(Corralx): Remove this version as it is duplicated
template<uint32_t kernel_size>
std::array<float, kernel_size> generate_gaussian_kernel_1d(float sigma)
{
	const int32_t kernel_half_size = static_cast<int32_t>(kernel_size) / 2;
	std::array<float, kernel_size> kernel;
	float weight_sum = 0;

	auto gauss_generator = [](float x, float sigma)
	{
		float c = 2.f * sigma * sigma;
		return std::exp(-x * x / c) / std::sqrt(c * (float)PI);
	};

	// kernel generation
	for (int32_t i = 0; i < static_cast<int32_t>(kernel_size); ++i)
	{
		float value = gauss_generator(static_cast<float>(i - kernel_half_size), sigma);
		kernel[i] = value;
		weight_sum += value;
	}

	// Normalization
	for (uint32_t i = 0; i < kernel_size; ++i)
		kernel[i] /= weight_sum;

	return std::move(kernel);
}

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

// TODO(Corralx): Check if std::ref on ref params is still needed (it shouldn't)
template<typename Func, typename ...Args>
std::future<void> async_apply(Func f, Args... args)
{
	std::promise<void> promise;
	std::future<void> future = promise.get_future();
	std::thread(f, std::forward<Args>(args)..., std::move(promise)).detach();

	return future;
}
