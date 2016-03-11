#pragma once

#include <cstdint>
#include <limits>

#include "glm/glm.hpp"

constexpr uint32_t INVALID_TEXTURE_ID = std::numeric_limits<uint32_t>::max();

struct material_t
{
	material_t() : color(.0f), texture_id(INVALID_TEXTURE_ID) {}
	material_t(glm::vec3 c, uint32_t id) : color(c), texture_id(id) {}

	glm::vec3 color;
	uint32_t texture_id;
};
