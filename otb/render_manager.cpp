#include "render_manager.hpp"

#include "utils.hpp"
#include "configuration.hpp"

#include "GL/gl3w.h"
#include "glm/gtc/type_ptr.hpp"

#include <cassert>

bool render_manager::init()
{
	if (!load_programs())
		return false;

	// TODO(Corralx): Init the rest?

	return true;
}

// TODO(Corralx): We shouldn't need the matrices_index in the program
bool render_manager::load_programs()
{
	{
		auto program = load_program(global_config.shader_path / global_config.base_vertex_shader,
									global_config.shader_path / global_config.base_fragment_shader);
		if (!program)
			return false;

		_base_program.id = program.value();
		_base_program.color_location = glGetUniformLocation(_base_program.id, "color");
		glUniformBlockBinding(_base_program.id, glGetUniformBlockIndex(_base_program.id, "Matrices"), 0);
	}

	{
		auto program = load_program(global_config.shader_path / global_config.occlusion_vertex_shader,
									global_config.shader_path / global_config.occlusion_fragment_shader);
		if (!program)
			return false;

		_occlusion_program.id = program.value();
		_occlusion_program.texture_location = glGetUniformLocation(_occlusion_program.id, "occlusion_map");
		glUniformBlockBinding(_occlusion_program.id, glGetUniformBlockIndex(_occlusion_program.id, "Matrices"), 0);
	}

	{
		auto program = load_program(global_config.shader_path / global_config.wireframe_vertex_shader,
									global_config.shader_path / global_config.wireframe_fragment_shader,
									global_config.shader_path / global_config.wireframe_geometry_shader);
		if (!program)
			return false;

		_wireframe_program.id = program.value();
		glUniformBlockBinding(_wireframe_program.id, glGetUniformBlockIndex(_wireframe_program.id, "Matrices"), 0);
	}

	return true;
}

void render_manager::render(const std::vector<mesh_t>& scene)
{
	for (auto& mesh : scene)
	{
		auto& material = mesh.material();
		if (material.state == material_t::state_t::HIDDEN)
			continue;

		switch (material.state)
		{
			case material_t::state_t::BASE:
				_base_geometry.emplace_back((uint32_t)mesh.faces().size(),
					_binding_mgr[mesh].base_vao, material);
				return;

			case material_t::state_t::OCCLUSION:
				_occlusion_geometry.emplace_back((uint32_t)mesh.faces().size(),
					_binding_mgr[mesh].occlusion_vao, material);
				return;

			case material_t::state_t::WIREFRAME:
				_wireframe_geometry.emplace_back((uint32_t)mesh.faces().size(),
					_binding_mgr[mesh].wireframe_vao, material);
				return;

			default:
				assert(false);
				return;
		}
	}

	update_matrices();

	render_base_geometry();
	render_occlusion_geometry();
	render_wireframe_geometry();

	assert(glGetError() == GL_NO_ERROR);
}

void render_manager::render_base_geometry()
{
	glUseProgram(_base_program.id);

	for (auto& geom : _base_geometry)
	{
		glBindVertexArray(geom.vao);
		glUniform3fv(_base_program.color_location, 1, glm::value_ptr(geom.material.color));
		glDrawElements(GL_TRIANGLES, geom.primitive_num * 3, GL_UNSIGNED_INT, nullptr);
	}
}

void render_manager::render_occlusion_geometry()
{
	glUseProgram(_occlusion_program.id);
	glActiveTexture(GL_TEXTURE0);

	for (auto& geom : _occlusion_geometry)
	{
		glBindVertexArray(geom.vao);
		glBindTexture(GL_TEXTURE_2D, geom.material.texture_id);
		glUniform1i(_occlusion_program.texture_location, 0);
		glDrawElements(GL_TRIANGLES, geom.primitive_num * 3, GL_UNSIGNED_INT, nullptr);
	}
}

void render_manager::render_wireframe_geometry()
{
	glUseProgram(_wireframe_program.id);

	for (auto& geom : _wireframe_geometry)
	{
		glBindVertexArray(geom.vao);
		glDrawElements(GL_TRIANGLES, geom.primitive_num * 3, GL_UNSIGNED_INT, nullptr);
	}
}

void render_manager::update_matrices()
{
	// TODO(Corralx)
}
