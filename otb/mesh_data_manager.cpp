#include "mesh_data_manager.hpp"
#include "mesh.hpp"

#include "GL/gl3w.h"

#include <cassert>

void mesh_data_manager::bind_data(const mesh_t& mesh, size_t vertices_index, size_t normals_index,
								  size_t tex_coords_index, size_t faces_index)
{
	uint32_t vaos[3];
	glGenVertexArrays(3, vaos);

	// Base VAO
	glBindVertexArray(vaos[0]);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	glBindBuffer(GL_ARRAY_BUFFER, _buffer_mgr[vertices_index]);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
	glBindBuffer(GL_ARRAY_BUFFER, _buffer_mgr[normals_index]);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _buffer_mgr[faces_index]);

	// Occlusion VAO
	glBindVertexArray(vaos[1]);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	glBindBuffer(GL_ARRAY_BUFFER, _buffer_mgr[vertices_index]);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
	glBindBuffer(GL_ARRAY_BUFFER, _buffer_mgr[tex_coords_index]);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _buffer_mgr[faces_index]);

	// Wireframe VAO
	glBindVertexArray(vaos[2]);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, _buffer_mgr[vertices_index]);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _buffer_mgr[faces_index]);

	_data.insert(_data.begin() + mesh.index,
	{
		vaos[0], vaos[1], vaos[2],
		vertices_index, normals_index, tex_coords_index, faces_index
	});

	glBindVertexArray(0);
	assert(glGetError() == GL_NO_ERROR);
}

mesh_data_manager::data mesh_data_manager::get(const mesh_t& mesh) const
{
	return _data[mesh.index];
}

mesh_data_manager::data mesh_data_manager::operator[](const mesh_t& mesh) const
{
	return _data[mesh.index];
}

mesh_data_manager::data mesh_data_manager::get(uint32_t mesh_index) const
{
	return _data[mesh_index];
}

mesh_data_manager::data mesh_data_manager::operator[](uint32_t mesh_index) const
{
	return _data[mesh_index];
}
