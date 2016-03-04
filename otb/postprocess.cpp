#include "postprocess.hpp"
#include "utils.hpp"

#include <thread>

using image_f32 = image<image_format::F32>;

static void invert_helper(image_f32& image, std::promise<void> promise)
{
	const uint32_t h = image.height();
	const uint32_t w = image.width();

	for (uint32_t i = 0; i < h; ++i)
	{
		for (uint32_t j = 0; j < w; ++j)
		{
			uint32_t index = i * w + j;
			float value = image[index];

			image[index] = saturate(1.f - value);
		}
	}

	promise.set_value_at_thread_exit();
}

/*
std::future<void> invert(image_f32& image)
{
	std::promise<void> promise;
	std::future<void> future;
	std::thread([](image_f32& image, std::promise<void> promise) { invert_helper(image, std::move(promise)); },
				std::ref(image), std::move(promise)).detach();

	return std::move(future);
}
*/

std::future<void> invert(image_f32& image)
{
	return async_apply(invert_helper, std::ref(image));
}

static void gaussian_blur_helper(image_f32& image, uint32_t num_pass, uint32_t kernel_size, float sigma, std::promise<void> promise)
{
	const uint32_t h = image.height();
	const uint32_t w = image.width();

	image_f32 temp(w, h);
	const int32_t kernel_half_width = static_cast<int32_t>(kernel_size) / 2;

	const std::vector<float> gaussian_kernel = generate_gaussian_kernel_1d(sigma, kernel_size);

	for (uint32_t pass = 0; pass < num_pass; ++pass)
	{
		// First pass: blur horizontally
		for (uint32_t i = 0; i < h; ++i)
		{
			for (uint32_t j = 0; j < w; ++j)
			{
				float sum = .0f;
				for (int32_t k = -kernel_half_width; k <= kernel_half_width; ++k)
				{
					const uint32_t sample_j = clamp(j + k, (uint32_t)0, w - 1);
					const uint32_t index = i * w + sample_j;
					sum += image[index] * gaussian_kernel[k + kernel_half_width];
				}

				const uint32_t index = i * w + j;
				temp[index] = sum;
			}
		}

		// Second pass: blur vertically
		for (uint32_t j = 0; j < w; ++j)
		{
			for (uint32_t i = 0; i < h; ++i)
			{
				float sum = .0f;
				for (int32_t k = -kernel_half_width; k <= kernel_half_width; ++k)
				{
					const uint32_t sample_i = clamp(i + k, (uint32_t)0, h - 1);
					const uint32_t index = sample_i * w + j;
					sum += temp[index] * gaussian_kernel[k + kernel_half_width];
				}

				const uint32_t index = i * w + j;
				image[index] = sum;
			}
		}
	}

	promise.set_value_at_thread_exit();
}

/*
std::future<void> gaussian_blur(image_f32& image, uint8_t num_pass, uint8_t kernel_size, float sigma = 1.f)
{
	std::promise<void> promise;
	std::future<void> future;
	std::thread([](image_f32& image, uint8_t num_pass, uint8_t kernel_size, float sigma, std::promise<void> promise)
				{ gaussian_blur_helper(image, num_pass, kernel_size, sigma, std::move(promise)); },
				std::ref(image), num_pass, kernel_size, sigma, std::move(promise)).detach();

	return std::move(future);
}
*/

std::future<void> gaussian_blur(image_f32& image, uint8_t num_pass, uint8_t kernel_size, float sigma = 1.f)
{
	return async_apply(gaussian_blur_helper, std::ref(image), num_pass, kernel_size, sigma);
}

static void dither_helper(image_f32& image, std::promise<void> promise)
{
	const uint32_t h = image.height();
	const uint32_t w = image.width();

	// TODO(Corralx)

	promise.set_value_at_thread_exit();
}

/*
std::future<void> dither(image_f32& image)
{
	std::promise<void> promise;
	std::future<void> future;
	std::thread([](image_f32& image, std::promise<void> promise) { dither_helper(image, std::move(promise)); },
				std::ref(image), std::move(promise)).detach();

	return std::move(future);
}
*/

std::future<void> dither(image_f32& image)
{
	return async_apply(dither_helper, std::ref(image));
}
