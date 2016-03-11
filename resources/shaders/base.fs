#version 330 core

in vec2 uvs;

uniform sampler2D occlusion_map;

layout(location = 0) out vec4 out_color;

void main()
{
	float occlusion = texture(occlusion_map, uvs).r;
	out_color = vec4(occlusion, occlusion, occlusion, 1.0);
}
