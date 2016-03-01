#pragma once

#include <cstdint>
#include <vector>
#include <limits>
#include <array>

#include "glm/glm.hpp"

class mesh_t;
struct __RTCDevice;
struct __RTCScene;

namespace embree
{

struct vertex
{
	float x;
	float y;
	float z;
	float _padding;
};

struct triangle
{
	int32_t v0;
	int32_t v1;
	int32_t v2;
};

using mesh_id = uint32_t;

const mesh_id NO_HIT_ID = std::numeric_limits<mesh_id>::max();

struct intersect_result
{
	std::array<mesh_id, 8> ids;
	std::array<float, 8> distances;
};

using occluded_result = std::array<bool, 8>;

struct ray
{
	std::array<glm::vec3, 8> positions;
	std::array<glm::vec3, 8> directions;
};

// TODO(Corralx): Support ray masking
class context
{
public:
	context();
	~context();

	context(const context&) = delete;
	context(context&&) = default;

	context& operator=(const context&) = delete;
	context& operator=(context&&) = default;

	mesh_id add_mesh(const mesh_t& mesh);
	void remove_mesh(mesh_id id);

	bool commit();

	intersect_result intersect(const ray& r, float max_distance, float min_distance = .0001f);
	occluded_result occluded(const ray& r, float max_distance, float min_distance = .0001f);

private:
	__RTCDevice* _device;
	__RTCScene* _scene;

	std::vector<mesh_id> _geometry;
};

}
