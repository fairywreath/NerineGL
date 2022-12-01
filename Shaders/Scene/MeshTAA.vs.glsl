/*
 * Render scene + velocity buffer.
 */

#version 460 core

#extension GL_ARB_gpu_shader_int64 : enable

#include "Shaders/Include/SceneData.inc.glsl"
#include "Shaders/Include/TAAFrameData.inc.glsl"

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

layout(location = 5) out VelocityData out_VelocityData;

const mat4 scaleBias
    = mat4(0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 0.0, 0.5, 0.5, 0.5, 1.0);

// float4x4 currentFrame_modelMatrix = actorParams.modelMatrix;
// float4 currentFrame_worldPos  = currentFrame_modelMatrix * float4(in.position, 1.0);
// float4 currentFrame_clipPos = viewportParams.viewProjectionMatrix * currentFrame_worldPos;

// out.currentFramePosition = currentFrame_clipPos;

// float4 currentFrame_clipPos_jittered =
//     currentFrame_clipPos + float4(viewportParams.jitter*currentFrame_clipPos.w,0,0);

// out.position = currentFrame_clipPos_jittered;

// float4x4 prevFrame_modelMatrix = actorParams.prevModelMatrix;
// float4 prevFrame_worldPos  = prevFrame_modelMatrix * float4(in.position, 1.0);
// float4 prevFrame_clipPos = viewportParams.prevViewProjMatrix * prevFrame_worldPos;

// out.prevFramePosition = prevFrame_clipPos;

void main()
{
    mat4 model = _models[gl_BaseInstance >> 16];
    mat4 mvp = proj * view * model;

    vec4 clipPos = mvp * vec4(in_Vertex, 1.0);

    // Jitter.
    vec4 clipPosJittered = clipPos + (vec4(taaJitterOffset, 0.0, 0.0) * clipPos.w);

    gl_Position = clipPosJittered;

    out_TexCoord = in_TexCoord;
    out_WorldNormal = transpose(inverse(mat3(model))) * in_Normal;
    out_WorldPos = (view * vec4(in_Vertex, 1.0)).xyz;
    out_MaterialIndex = gl_BaseInstance & 0xffff;
    out_ShadowCoord = scaleBias * light * model * vec4(in_Vertex, 1.0);

    // Setup data for velocity calculation.
    out_VelocityData.currentPos = clipPos;

    // XXX: Need an extra SSBO for this? Currently does not handle change in world model matrices.
    mat4 prevModel = model;
    out_VelocityData.prevPos = prevProj * prevView * prevModel * vec4(in_Vertex, 1.0);
}
