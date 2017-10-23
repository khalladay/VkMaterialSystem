#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0, set = 0) uniform INST_STATIC
{
	vec4 tint;
	vec3 tint2;
}static_data;

layout(binding = 1, set = 0) uniform sampler2D texSampler;


layout(location = 0) out vec4 outColor;
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragUV;

void main() 
{
	vec4 tex = texture(texSampler, fragUV);
    outColor = tex * vec4(fragColor * static_data.tint.xyz * static_data.tint2, 1.0);
}