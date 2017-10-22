#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(binding = 0, set = 0) uniform INST_STATIC
{
	vec4 tint;
	vec3 tint2;
}dyn;

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec3 fragColor;

void main() 
{
    outColor = vec4(fragColor *  dyn.tint.xyz * dyn.tint2, 1.0);
}