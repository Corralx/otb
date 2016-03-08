#version 330 core

layout(location = 0) vec3 position;
layout(location = 1) vec3 normal;
layout(location = 2) vec2 coords;

out vec2 uvs;

void main()
{
	uvs = coords;
	gl_Position = vec4(position, 1.0);
}
