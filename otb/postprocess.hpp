#pragma once

#include <cstdint>
#include <future>

#include "image.hpp"
#include "utils.hpp"

std::future<void> invert(image<pixel_format::F32>& image);

std::future<void> gaussian_blur(image<pixel_format::F32>& image, uint32_t num_pass, uint32_t kernel_size, float sigma = 1.f);

// TODO(Corralx): Figure out good parametrization for this
std::future<void> dither(image<pixel_format::F32>& image);
