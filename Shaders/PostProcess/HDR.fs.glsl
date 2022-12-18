/*
 * HDR tone mapping.
 */

#version 460 core

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 out_FragColor;

layout(binding = 0) uniform sampler2D _TextureScene;
layout(binding = 1) uniform sampler2D _TextureLuminance;
layout(binding = 2) uniform sampler2D _TextureBloom;

layout(std140, binding = 0) uniform HDRParams
{
    float exposure;
    float maxWhite;
    float bloomStrength;
};

// Extended Reinhard tone mapping.
vec3 Reinhard2(vec3 x)
{
    return (x * (1.0 + x / (maxWhite * maxWhite))) / (1.0 + x);
}

void main()
{
    vec3 color = texture(_TextureScene, uv).rgb;
    vec3 bloom = texture(_TextureBloom, uv).rgb;
    float avgLuminance = texture(_TextureLuminance, vec2(0.5, 0.5)).x;

    float midGray = 0.5;

    color *= exposure * midGray / (avgLuminance + 0.001);

    // XXX: Extra emissive strength?

    // color = Reinhard2(color);
    out_FragColor = vec4(color + bloomStrength * bloom, 1.0);
}
