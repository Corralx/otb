#pragma once

namespace elk
{
class path;
}

#include <vector>
#include <cstdint>
#include <limits>
#include <future>

class mesh_t;

class binding_manager
{
	struct data
	{
		uint32_t base_vao;		// Binds positions and normals
		uint32_t occlusion_vao; // Binds positions and texture coordinates
		uint32_t wireframe_vao; // Binds just the positions
	};

public:
	binding_manager() = default;
	// TODO(Corralx): Remove every active gl resources?
	~binding_manager() = default;

	binding_manager(const binding_manager&) = delete;
	binding_manager(binding_manager&&) = default;
	binding_manager& operator=(const binding_manager&) = delete;
	binding_manager& operator=(binding_manager&&) = default;

	// NOTE(Corralx): This creates the vaos and sets up the binding
	// This MUST be called from the main thread as VAOs are not shared between contexts
	void bind_data(const mesh_t&, uint32_t vertices_buffer, uint32_t normals_buffer,
				   uint32_t tex_coords_buffer, uint32_t faces_buffer);

	data get(const mesh_t&) const;
	data operator[](const mesh_t&) const;

	data get(uint32_t mesh_index) const;
	data operator[](uint32_t mesh_index) const;

private:
	std::vector<data> _data;
};
