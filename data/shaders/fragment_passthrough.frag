#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1, set = 2) uniform sampler2D texSampler;

layout(binding = 0, set = 0) uniform Globals
{
	float time;
	vec4 mouse;
}global;

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUV;

void main() 
{
	vec4 tex = texture(texSampler, fragUV);
	//tex.rgb *= fragColor;
    outColor = tex * fragColor;
}