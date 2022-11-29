#version 460 core

#extension GL_ARB_gpu_shader_int64 : enable

#include "Shaders/Include/SceneData.inc.glsl"

layout(std430, binding = 1) restrict readonly buffer Matrices
{
    mat4 _models[];
};

layout(location = 0) in vec3 in_Vertex;
layout(location = 1) in vec2 in_TexCoord;
layout(location = 2) in vec3 in_Normal;

layout(location = 0) out vec2 out_TexCoord;
layout(location = 1) out vec3 out_WorldNormal;
layout(location = 2) out vec3 out_WorldPos;
layout(location = 3) out flat uint out_MaterialIndex;
layout(location = 4) out vec4 out_ShadowCoord;

const mat4 scaleBias
    = mat4(0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 0.0, 0.5, 0.5, 0.5, 1.0);

void main()
{
    mat4 model = _models[gl_BaseInstance >> 16];
    mat4 mvp = proj * view * model;

    vec4 clipPos = mvp * vec4(in_Vertex, 1.0);

    // Jitter.
    clipPos += vec4(taaJitterOffset, 0.0, 0.0) * clipPos.w;

    gl_Position = clipPos;

    out_TexCoord = in_TexCoord;
    out_WorldNormal = transpose(inverse(mat3(model))) * in_Normal;
    out_WorldPos = (view * vec4(in_Vertex, 1.0)).xyz;
    out_MaterialIndex = gl_BaseInstance & 0xffff;
    out_ShadowCoord = scaleBias * light * model * vec4(in_Vertex, 1.0);
}