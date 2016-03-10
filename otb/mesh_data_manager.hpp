#pragma once

#include "buffer_manager.hpp"

namespace elk
{
class path;
}

#include <vector>
#include <cstdint>
#include <limits>
#include <future>

class mesh_t;

class mesh_data_manager
{
	struct data
	{
		uint32_t base_vao;		// Binds positions and normals
		uint32_t occlusion_vao; // Binds positions and texture coordinates
		uint32_t wireframe_vao; // Binds just the positions

		size_t vertices_index;
		size_t normals_index;
		size_t tex_coords_index;
		size_t faces_index;
	};

public:
	mesh_data_manager(buffer_manager& buffer_mgr) : _buffer_mgr(buffer_mgr), _data() {}
	// TODO(Corralx): Remove every active gl resources?
	~mesh_data_manager() = default;

	mesh_data_manager(const mesh_data_manager&) = delete;
	mesh_data_manager(mesh_data_manager&&) = default;
	mesh_data_manager& operator=(const mesh_data_manager&) = delete;
	mesh_data_manager& operator=(mesh_data_manager&&) = default;

	// NOTE(Corralx): This creates the vaos and sets up the binding
	// This MUST be called from the main thread as VAOs are not shared between contexts
	void bind_data(const mesh_t&, size_t vertices_index, size_t normals_index,
				   size_t tex_coords_index, size_t faces_index);

	data get(const mesh_t&) const;
	data operator[](const mesh_t&) const;

	data get(uint32_t mesh_index) const;
	data operator[](uint32_t mesh_index) const;

private:
	buffer_manager& _buffer_mgr;
	std::vector<data> _data;
};
