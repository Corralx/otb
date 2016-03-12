#pragma once

#include <cstdint>
#include <limits>

#include "glm/glm.hpp"

constexpr uint32_t INVALID_TEXTURE_ID = std::numeric_limits<uint32_t>::max();

struct material_t
{
	enum class state_t : uint8_t
	{
		BASE = 0,
		OCCLUSION = 1,
		WIREFRAME = 2,
		HIDDEN = 3
	};

	material_t() : color(.0f), texture_id(INVALID_TEXTURE_ID), state(state_t::BASE) {}
	material_t(glm::vec3 c, uint32_t id, state_t s) : color(c), texture_id(id), state(s) {}

	glm::vec3 color;
	uint32_t texture_id;
	state_t state;
};
