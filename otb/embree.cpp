#include "embree.hpp"
#include "mesh.hpp"
#include "elektra/platforms.hpp"

// TODO(Corralx): What about other os'es?
#ifdef ELK_PLATFORM_WINDOWS
#include <xmmintrin.h>
#include <pmmintrin.h>
#endif

#pragma warning (push, 0)
#include "embree2/rtcore.h"
#include "embree2/rtcore_ray.h"
#pragma warning (pop)

#include <cassert>

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

struct RTCORE_ALIGN(16) ray_mask
{
	uint32_t _[8];
};

static const ray_mask mask =
{
	0xFFFFFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF,
	0xFFFFFFFF
};

context::context()
{
	// Setting the CPU register flags
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
	_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);

	_device = rtcNewDevice();
	_scene = rtcDeviceNewScene(_device, RTC_SCENE_STATIC | RTC_SCENE_INCOHERENT |
							   RTC_SCENE_HIGH_QUALITY | RTC_SCENE_ROBUST, RTC_INTERSECT8);

	assert(_scene);
	assert(_device);
}

context::~context()
{
	for (mesh_id id : _geometry)
		rtcDeleteGeometry(_scene, id);

	assert(_scene);
	assert(_device);

	rtcDeleteScene(_scene);
	rtcDeleteDevice(_device);
}

mesh_id context::add_mesh(const mesh_t& mesh)
{
	size_t num_indices = mesh.faces().size();
	size_t num_vertices = mesh.vertices().size();

	assert(num_indices > 0);
	assert(num_vertices > 0);

	mesh_id id = rtcNewTriangleMesh(_scene, RTC_GEOMETRY_STATIC, num_indices, num_vertices);

	auto& vertices = mesh.vertices();
	vertex* embree_vertices = (vertex*)rtcMapBuffer(_scene, id, RTC_VERTEX_BUFFER);
	for (uint32_t i = 0; i < num_vertices; ++i)
	{
		embree_vertices[i].x = vertices[i].x;
		embree_vertices[i].y = vertices[i].y;
		embree_vertices[i].z = vertices[i].z;
		embree_vertices[i]._padding = .0f;
	}
	rtcUnmapBuffer(_scene, id, RTC_VERTEX_BUFFER);

	auto& faces = mesh.faces();
	triangle* embree_triangles = (triangle*)rtcMapBuffer(_scene, id, RTC_INDEX_BUFFER);
	for (uint32_t i = 0; i < num_indices; ++i)
	{
		embree_triangles[i].v0 = faces[i].v0;
		embree_triangles[i].v1 = faces[i].v1;
		embree_triangles[i].v2 = faces[i].v2;
	}
	rtcUnmapBuffer(_scene, id, RTC_INDEX_BUFFER);

	_geometry.push_back(id);
	return id;
}

void context::remove_mesh(mesh_id id)
{
	auto pos = std::find(std::begin(_geometry), std::end(_geometry), id);
	assert(pos != std::end(_geometry));

	_geometry.erase(pos);
	rtcDeleteGeometry(_scene, id);
}

bool context::commit()
{
	rtcCommit(_scene);
	return rtcDeviceGetError(_device) == RTC_NO_ERROR;
}

intersect_result context::intersect(const ray& r, float max_distance, float min_distance)
{
	RTCRay8 ray8{};
	for (uint32_t ray_id = 0; ray_id < 8; ++ray_id)
	{
		ray8.orgx[ray_id] = r.positions[ray_id].x;
		ray8.orgy[ray_id] = r.positions[ray_id].y;
		ray8.orgz[ray_id] = r.positions[ray_id].z;

		ray8.dirx[ray_id] = r.directions[ray_id].x;
		ray8.diry[ray_id] = r.directions[ray_id].y;
		ray8.dirz[ray_id] = r.directions[ray_id].z;

		ray8.tnear[ray_id] = min_distance;
		ray8.tfar[ray_id] = max_distance;

		ray8.geomID[ray_id] = NO_HIT_ID;
	}

	rtcIntersect8(&mask, _scene, ray8);

	intersect_result res{};
	for (uint32_t ray_id = 0; ray_id < 8; ++ray_id)
	{
		res.ids[ray_id] = ray8.geomID[ray_id];
		res.distances[ray_id] = ray8.tfar[ray_id];
	}

	return std::move(res);
}

occluded_result context::occluded(const ray& r, float max_distance, float min_distance)
{
	RTCRay8 ray8{};
	for (uint32_t ray_id = 0; ray_id < 8; ++ray_id)
	{
		ray8.orgx[ray_id] = r.positions[ray_id].x;
		ray8.orgy[ray_id] = r.positions[ray_id].y;
		ray8.orgz[ray_id] = r.positions[ray_id].z;
		
		ray8.dirx[ray_id] = r.directions[ray_id].x;
		ray8.diry[ray_id] = r.directions[ray_id].y;
		ray8.dirz[ray_id] = r.directions[ray_id].z;

		ray8.tnear[ray_id] = min_distance;
		ray8.tfar[ray_id] = max_distance;

		ray8.geomID[ray_id] = NO_HIT_ID;
	}

	rtcOccluded8(&mask, _scene, ray8);

	occluded_result res{};
	for (uint32_t ray_id = 0; ray_id < 8; ++ray_id)
		res[ray_id] = (ray8.geomID[ray_id] != NO_HIT_ID) ? true : false;

	return std::move(res);
}

}
