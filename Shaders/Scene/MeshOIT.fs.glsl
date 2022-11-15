
#version 460 core

#extension GL_ARB_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : enable

#include "Shaders/Include/FragmentCalculations.inc.glsl"
#include "Shaders/Include/MaterialData.inc.glsl"
#include "Shaders/Include/TransparentFragment.inc.glsl"

layout(early_fragment_tests) in;

#include "Shaders/Include/SceneData.inc.glsl"

layout(std430, binding = 2) restrict readonly buffer Materials
{
    MaterialData _Materials[];
};

layout(binding = 5) uniform samplerCube _TextureEnvMap;
layout(binding = 6) uniform samplerCube _TextureEnvMapIrradiance;
layout(binding = 7) uniform sampler2D _TextureBRDFLut;

layout(location = 0) in vec2 in_TexCoord;
layout(location = 1) in vec3 in_WorldNormal;
layout(location = 2) in vec3 in_WorldPos;
layout(location = 3) in flat uint in_MaterialIndex;

layout(location = 0) out vec4 out_FragColor;

layout(binding = 0, r32ui) uniform uimage2D _Heads;
layout(binding = 0, offset = 0) uniform atomic_uint _NumFragments;

layout(std430, binding = 3) buffer Lists
{
    TransparentFragment _Fragments[];
};

void main()
{
    MaterialData material = _Materials[in_MaterialIndex];

    vec4 albedo = material.albedoColor;
    vec3 normalSample = vec3(0.0, 0.0, 0.0);

    if (material.albedoMap > 0)
    {
        albedo = texture(sampler2D(unpackUint2x32(material.albedoMap)), in_TexCoord);
    }
    if (material.normalMap > 0)
    {
        normalSample = texture(sampler2D(unpackUint2x32(material.normalMap)), in_TexCoord).xyz;
    }

    vec3 worldNormal = normalize(in_WorldNormal);

    // Normal mapping.
    if (length(normalSample) > 0.5)
    {
        worldNormal = PerturbNormal(worldNormal, normalize(cameraPos.xyz - in_WorldPos.xyz),
                                    normalSample, in_TexCoord);
    }

    // Diffuse-only IBL.
    vec3 f0 = vec3(0.04);
    vec3 diffuseColor = albedo.rgb * (vec3(1.0) - f0);
    vec3 diffuse = texture(_TextureEnvMapIrradiance, in_WorldNormal.xyz).rgb * diffuseColor;

    // Minor reflections from the env. map for transparent objects.
    vec3 v = normalize(cameraPos.xyz - in_WorldPos);
    vec3 reflection = reflect(v, worldNormal);
    vec3 colorReflection = texture(_TextureEnvMap, reflection).rgb;

    out_FragColor = vec4(diffuse + colorReflection, 1.0);

    // Order-Independent Transparency.
    // https://fr.slideshare.net/hgruen/oit-and-indirect-illumination-using-dx11-linked-lists
    float alpha = clamp(albedo.a, 0.0, 1.0) * material.transparencyFactor;
    bool isTransparent = alpha < 0.99;
    if (isTransparent && gl_HelperInvocation == false)
    {
        if (alpha > 0.01)
        {
            uint index = atomicCounterIncrement(_NumFragments);
            const uint maxOITfragments = 16 * 1024 * 1024;
            if (index < maxOITfragments)
            {
                uint prevIndex = imageAtomicExchange(_Heads, ivec2(gl_FragCoord.xy), index);
                _Fragments[index].color = vec4(out_FragColor.rgb, alpha);
                _Fragments[index].depth = gl_FragCoord.z;
                _Fragments[index].next = prevIndex;
            }
        }
    }
};
