#include "mesh.hpp"

#include "GL/gl3w.h"

mesh_t::~mesh_t()
{
	// Release gl resources if any
	glDeleteBuffers(4, _buffers);
	glDeleteVertexArrays(1, &_vao);
}

void mesh_t::init_buffers()
{
	glGenVertexArrays(1, &_vao);
	glBindVertexArray(_vao);

	// Buffers
	glGenBuffers(4, _buffers);

	// Vertices
	glBindBuffer(GL_ARRAY_BUFFER, _buffers[0]);
	glBufferData(GL_ARRAY_BUFFER, 3 * sizeof(float) * _vertices.size(), _vertices.data(), GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

	// Normals
	glBindBuffer(GL_ARRAY_BUFFER, _buffers[1]);
	glBufferData(GL_ARRAY_BUFFER, 3 * sizeof(float) * _normals.size(), _normals.data(), GL_STATIC_DRAW);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

	// Texture Coords
	glBindBuffer(GL_ARRAY_BUFFER, _buffers[2]);
	glBufferData(GL_ARRAY_BUFFER, 2 * sizeof(float) * _coords.size(), _coords.data(), GL_STATIC_DRAW);
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

	// Indices
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _buffers[3]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 3 * sizeof(uint32_t) * _faces.size(), _faces.data(), GL_STATIC_DRAW);

	assert(glGetError() == GL_NO_ERROR);

	glBindVertexArray(0);
}
