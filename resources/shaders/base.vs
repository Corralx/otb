#version 330 core

layout(location = 0) vec3 position;
layout(location = 1) vec3 normal;
layout(location = 2) vec2 coords;

uniform mat4 proj;
uniform mat4 view;

out vec2 uvs;

void main()
{
	uvs = coords;
	gl_Position = proj * view * vec4(position, 1.0);
}
