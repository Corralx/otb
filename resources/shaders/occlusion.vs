#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 coords;

// Fully specified layout to avoid querying for offsets
layout(std140) uniform Matrices
{
    mat4 proj;
    mat4 view;
	mat4 view_proj;
};

out vec2 uvs;

void main()
{
	uvs = coords;
	gl_Position = view_proj * vec4(position, 1.0);
}