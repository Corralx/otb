#include "binding_manager.hpp"
#include "mesh.hpp"

#include "GL/gl3w.h"

#include <cassert>

void binding_manager::bind_data(const mesh_t& mesh, uint32_t vertices_buffer, uint32_t normals_buffer,
								uint32_t tex_coords_buffer, uint32_t faces_buffer)
{
	uint32_t vaos[3];
	glGenVertexArrays(3, vaos);

	// Base VAO
	glBindVertexArray(vaos[0]);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	glBindBuffer(GL_ARRAY_BUFFER, vertices_buffer);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
	glBindBuffer(GL_ARRAY_BUFFER, normals_buffer);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, faces_buffer);

	// Occlusion VAO
	glBindVertexArray(vaos[1]);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	glBindBuffer(GL_ARRAY_BUFFER, vertices_buffer);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
	glBindBuffer(GL_ARRAY_BUFFER, tex_coords_buffer);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, faces_buffer);

	// Wireframe VAO
	glBindVertexArray(vaos[2]);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, vertices_buffer);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, faces_buffer);

	// Resize if there is no enough space
	// insert(...) is supposed to resize for us but Microsoft STL doesn't seems to agree
	if (mesh.index >= _data.size())
		_data.resize(mesh.index + 1);
	_data.insert(_data.begin() + mesh.index, { vaos[0], vaos[1], vaos[2] });

	glBindVertexArray(0);
	assert(glGetError() == GL_NO_ERROR);
}

binding_manager::data binding_manager::get(const mesh_t& mesh) const
{
	return _data[mesh.index];
}

binding_manager::data binding_manager::operator[](const mesh_t& mesh) const
{
	return _data[mesh.index];
}

binding_manager::data binding_manager::get(uint32_t mesh_index) const
{
	return _data[mesh_index];
}

binding_manager::data binding_manager::operator[](uint32_t mesh_index) const
{
	return _data[mesh_index];
}
