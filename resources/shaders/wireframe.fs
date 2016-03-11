#version 330 core

in vec3 barycentric;

layout(location = 0) out vec4 out_color;

// http://codeflow.org/entries/2012/aug/02/easy-wireframe-display-with-barycentric-coordinates/
float edge_factor()
{
	vec3 d = fwidth(barycentric);
	vec3 a3 = smoothstep(vec3(0.0), d * 1.5, barycentric);
	return min(min(a3.x, a3.y), a3.z);
}

void main()
{
	out_color.rgb = mix(vec3(0.0), vec3(0.5), edge_factor());

    if (gl_FrontFacing)
		out_color.a = (1.0 - edge_factor()) * 0.85;
    else
        out_color.a = (1.0 - edge_factor()) * 0.5;
}
