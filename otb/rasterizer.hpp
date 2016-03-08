#pragma once

#include <cstdint>
#include <future>

#include "image.hpp"

class mesh_t;

// When the future is ready, the image contains the UV triangle indices covering each pixel
/* NOTE(Corralx): Only the pixels covered by the UV unwrap are overwritten
   if a default value is needed, call initialize(...) on the image before submitting */
std::future<void> rasterize_triangle_software(const mesh_t& mesh, image<pixel_format::U32>& image, uint8_t supersampling = 2);
std::future<void> rasterize_triangle_hardware(const mesh_t& mesh, image<pixel_format::U32>& image);
