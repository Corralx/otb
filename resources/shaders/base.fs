#version 330 core

in vec2 uvs;

layout(location = 0) out vec4 out_color;

void main()
{
	out_color = vec4(0.0, uvs.x, uvs.y, 1.0);
}
