
#version 460 core

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 out_FragColor;

layout(binding = 0) uniform sampler2D _TextureScene;
layout(binding = 1) uniform sampler2D _TextureSSAO;

layout(std140, binding = 0) uniform SSAOParams
{
    float scale;
    float bias;
};

void main()
{
    vec4 color = texture(_TextureScene, uv);
    float ssao = clamp(texture(_TextureSSAO, uv).r + bias, 0.0, 1.0);

    out_FragColor = vec4(mix(color, color * ssao, scale).rgb, 1.0);
}
