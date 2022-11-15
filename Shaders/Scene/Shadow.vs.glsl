#version 460 core

#include "Shaders/Include/SceneData.inc.glsl"

layout(std430, binding = 1) restrict readonly buffer Matrices
{
    mat4 _Models[];
};

layout(location = 0) in vec3 in_Vertex;

void main()
{
    mat4 model = _Models[gl_BaseInstance >> 16];

    // XXX: proj and view have been modified to be the light source's inside the CPU program.
    // Maybe add additional params to the scene data instead of modifying it?
    gl_Position = proj * view * model * vec4(in_Vertex, 1.0);
}
