#version 460 core

#extension GL_ARB_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : enable

#include "Shaders/Include/FragmentCalculations.inc.glsl"
#include "Shaders/Include/MaterialData.inc.glsl"
#include "Shaders/Include/SceneData.inc.glsl"

layout(std430, binding = 2) restrict readonly buffer Materials
{
    MaterialData _Materials[];
};
layout(binding = 4) uniform sampler2D _TextureShadow;
layout(binding = 5) uniform samplerCube _TextureEnvMap;
layout(binding = 6) uniform samplerCube _TextureEnvMapIrradiance;
layout(binding = 7) uniform sampler2D _TextureBRDFLut;

layout(location = 0) in vec2 in_TexCoord;
layout(location = 1) in vec3 in_WorldNormal;
layout(location = 2) in vec3 in_WorldPos;
layout(location = 3) in flat uint in_MaterialIndex;
layout(location = 4) in vec4 in_ShadowCoord;

layout(location = 0) out vec4 out_FragColor;

// Percentage-closer filtering.
float PCF(int kernelSize, vec2 shadowCoord, float depth)
{
    float size = 1.0 / float(textureSize(_TextureShadow, 0).x);
    float shadow = 0.0;
    int range = kernelSize / 2;

    for (int v = -range; v <= range; v++)
    {
        for (int u = -range; u <= range; u++)
        {
            shadow += (depth >= texture(_TextureShadow, shadowCoord + size * vec2(u, v)).r) ? 1.0
                                                                                            : 0.0;
        }
    }

    return shadow / (kernelSize * kernelSize);
}

float ShadowFactor(vec4 shadowCoord)
{
    // Check if disabled.
    if (light[3][3] == 0.0)
    {
        return 1.0;
    }

    vec4 shadowCoordsDivided = shadowCoord / shadowCoord.w;

    if (abs(shadowCoordsDivided.z) < 1)
    {
        float depthBias = -0.0001;
        float shadowSample = PCF(13, shadowCoordsDivided.xy, shadowCoordsDivided.z + depthBias);
        return mix(1.0, 0.3, shadowSample);
    }

    return 1.0;
}

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

    RunAlphaTest(albedo.a, material.alphaTest);

    vec3 worldNormal = normalize(in_WorldNormal);

    // Normal mapping.
    if (length(normalSample) > 0.5)
    {
        worldNormal = PerturbNormal(worldNormal, normalize(cameraPos.xyz - in_WorldPos.xyz),
                                    normalSample, in_TexCoord);
    }

    /*
     * IBL calculations.
     */
    // f0: PBR specular reflectance at normal incidence (hardcoded here).
    vec3 f0 = vec3(0.04);
    vec3 diffuseColor = albedo.rgb * (vec3(1.0) - f0);
    vec3 envMapColor = texture(_TextureEnvMapIrradiance, worldNormal.xyz).rgb;

    // YYY: Hack to reduce very blue-ish color.
    envMapColor.b *= 0.72;

    vec3 diffuse = envMapColor * diffuseColor;
    // vec3 diffuse = diffuseColor;
    // vec3 diffuse = envMapColor;

    // Assign final fragment color.
    vec4 finalColor = vec4(diffuse * ShadowFactor(in_ShadowCoord), 1.0);

    if (material.emissiveMap > 0)
    {
        vec4 emissive = texture(sampler2D(unpackUint2x32(material.emissiveMap)), in_TexCoord);

        float baseEmissiveFactor = 0.3;
        // float baseEmissiveFactor = 0.45;

        baseEmissiveFactor *= emissiveMapStrength;

        finalColor.rgb += emissive.rgb * (baseEmissiveFactor);

        // Assign extra emissiveStrength on w channel? As bloom parameters?
        // float emissiveStrength = 1.1;
        // finalColor.w = emissiveStrength;
    }

    out_FragColor = finalColor;
}