#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 vertex;
layout(location = 1) in vec2 uv;

layout(location = 0) out vec4 fragColor;

out gl_PerVertex 
{
    vec4 gl_Position;
};

void main() 
{
    gl_Position = vec4(vertex, 1.0);
    fragColor = obj.col;
}

