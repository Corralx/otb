#pragma once

#include "program.hpp"
#include "binding_manager.hpp"
#include "image.hpp"
#include "mesh.hpp"
#include "material.hpp"

#include <cstdint>
#include <vector>

class render_manager
{
	struct data_t
	{
		data_t(uint32_t _prim_num, uint32_t _vao, const material_t& _mat) :
			primitive_num(_prim_num), vao(_vao), material(_mat) {}

		uint32_t primitive_num;
		uint32_t vao;
		const material_t& material;
	};

public:
	render_manager(binding_manager& bm) : _binding_mgr(bm), _base_program(), _occlusion_program(),
		_wireframe_program(), _base_geometry(), _occlusion_geometry(), _wireframe_geometry() {}
	// TODO(Corralx): Release programs?
	~render_manager() = default;

	bool init();

	// TODO(Corralx): Pass the camera
	void render(const std::vector<mesh_t>& scene);

private:
	bool load_programs();

	void render_base_geometry();
	void render_occlusion_geometry();
	void render_wireframe_geometry();

	// TODO(Corralx): Pass the camera
	void update_matrices();

	binding_manager& _binding_mgr;

	base_program_t _base_program;
	occlusion_program_t _occlusion_program;
	wireframe_program_t _wireframe_program;

	std::vector<data_t> _base_geometry;
	std::vector<data_t> _occlusion_geometry;
	std::vector<data_t> _wireframe_geometry;
};
