#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0, set = 0) uniform FRAG_IN
{
	vec4 tint;
}dyn;


layout(binding = 1, set = 0) uniform FRAG_IN2
{
	vec4 tint2;
}dyn2;

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec3 fragColor;

void main() 
{
    outColor = vec4(fragColor * dyn2.tint2.xyz *  dyn.tint.xyz, 1.0);
}