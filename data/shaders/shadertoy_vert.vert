#version 450 
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0, set = 0)uniform GLOBAL_DATA
{
	float time;
	vec4 mouse;
	vec2 resolution;
	mat4 viewMatrix;
	vec4 worldSpaceCameraPos;
}global;

layout(location = 0) in vec3 vertex;
layout(location = 1) in vec2 uv;
layout(location = 0) out vec2 fragUV;

out gl_PerVertex
{
    vec4 gl_Position;
};


void main()
{
	gl_Position = vec4(vertex, 1.0);
	fragUV = vec2(uv.x, 1.0-uv.y) * global.resolution;
}