#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1, set = 2) uniform sampler2D texSampler;
layout(binding = 0, set = 1) uniform sampler2D testSampler;

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUV;

void main() 
{
	vec4 tex = texture(texSampler, fragUV);
	vec4 tex2 = texture(testSampler, fragUV);
    outColor = mix(tex,tex2,fragColor.g);
}