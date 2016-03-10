#pragma once

#include "glm/glm.hpp"

#include "utils.hpp"

#include <vector>
#include <cstdint>
#include <future>

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

// NOTE(Corralx): This is just a container for the mesh data
class mesh_t
{
public:
	mesh_t(std::vector<vertex_t>&& vertices, std::vector<normal_t>&& normals,
		   std::vector<texture_coord_t>&& coords, std::vector<face_t>&& faces) :
		   index(generate_unique_index()), _vertices(vertices), _normals(normals),
		   _coords(coords), _faces(faces) {}

	mesh_t(const mesh_t&) = delete;
	mesh_t(mesh_t&&) = default;
	mesh_t& operator=(const mesh_t&) = delete;
	mesh_t& operator=(mesh_t&&) = default;

	// NOTE(Corralx): A mesh releases his buffers but not the associated gl resources
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

	const uint32_t index;

private:
	std::vector<vertex_t> _vertices;
	std::vector<normal_t> _normals;
	std::vector<texture_coord_t> _coords;
	std::vector<face_t> _faces;
};
