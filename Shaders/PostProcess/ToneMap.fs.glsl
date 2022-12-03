/*
 * HDR tone mapping.
 */

#version 460 core

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 out_FragColor;

layout(binding = 0) uniform sampler2D _TextureIn;

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
    vec3 color = texture(_TextureIn, uv).rgb;
    color = Reinhard2(color);

    out_FragColor = vec4(color, 1.0);
}
