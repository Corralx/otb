#pragma once

#include "glm/glm.hpp"

#include <vector>
#include <cstdint>

using vertex_t = glm::vec3;
using normal_t = glm::vec3;
using texture_coord_t = glm::vec2;

struct face_t
{
	uint32_t v0;
	uint32_t v1;
	uint32_t v2;
};

namespace elk
{
class path;
}

class mesh_t
{
	friend std::vector<mesh_t> load_mesh(const elk::path& path);

	mesh_t() = default;

public:
	mesh_t(const mesh_t&) = default;
	mesh_t(mesh_t&&) = default;
	mesh_t& operator=(const mesh_t&) = default;
	mesh_t& operator=(mesh_t&&) = default;
	~mesh_t() = default;

	const std::vector<vertex_t>& vertices() const
	{
		return _vertices;
	}

	const std::vector<normal_t>& normals() const
	{
		return _normals;
	}

	const std::vector<texture_coord_t>& texture_coords() const
	{
		return _coords;
	}

	const std::vector<face_t>& faces() const
	{
		return _faces;
	}

private:
	std::vector<vertex_t> _vertices;
	std::vector<normal_t> _normals;
	std::vector<texture_coord_t> _coords;
	std::vector<face_t> _faces;
};
