#version 460 core

#include "Shaders/Include/SceneData.inc.glsl"
#include "Shaders/Include/TAAFrameData.inc.glsl"

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_FragColor;

layout(binding = 0) uniform sampler2D _TextureCurrent;
layout(binding = 1) uniform sampler2D _TextureHistory;

layout(binding = 2) uniform sampler2D _TextureVelocity;

// XXX: Use these for a more advanced implementation.
// layout(binding = 3) uniform sampler2D _TextureDepth;
// layout(binding = 4) uniform sampler2D _TextureDepthHistory;
// layout(binding = 5) uniform sampler2D _TextureLuminanceCurrent;
// layout(binding = 6) uniform sampler2D _TextureLuminanceHistory;

layout(std140, binding = 11) uniform TAAParams
{
    uint haltonSequenceCount;
    float sourceWeight_;
    uint colorClampingType;
    uint luminanceWeigh;
};

float Luminance(vec3 v)
{
    return dot(v, vec3(0.2127, 0.7152, 0.0722));
}

float rcp(float i)
{
    return 1.0 / i;
}

void main()
{
    vec2 velocity = texture(_TextureVelocity, uv).xy;
    vec2 previousPixelPos = uv - velocity;
    vec4 colorCurrent = texture(_TextureCurrent, uv);
    vec4 colorHistory = texture(_TextureHistory, previousPixelPos);

    // Color clamp.
    vec3 colorMin = vec3(10000.0);
    vec3 colorMax = vec3(-10000.0);

    // 4 neighbours top/bot/left/right clamp.
    if (colorClampingType == 1 || colorClampingType == 3)
    {
        vec3 NearColor0 = textureOffset(_TextureCurrent, uv, ivec2(1, 0)).xyz;
        vec3 NearColor1 = textureOffset(_TextureCurrent, uv, ivec2(0, 1)).xyz;
        vec3 NearColor2 = textureOffset(_TextureCurrent, uv, ivec2(-1, 0)).xyz;
        vec3 NearColor3 = textureOffset(_TextureCurrent, uv, ivec2(0, -1)).xyz;

        colorMin
            = min(colorCurrent.rgb, min(NearColor0, min(NearColor1, min(NearColor2, NearColor3))));
        colorMax
            = max(colorCurrent.rgb, max(NearColor0, max(NearColor1, max(NearColor2, NearColor3))));

        // colorHistory = vec4(clamp(colorHistory.rgb, colorMin, colorMax), 1.0);
    }
    // 3x3 neighbors clamp.
    else if (colorClampingType == 2)
    {
        for (int x = -1; x <= 1; ++x)
        {
            for (int y = -1; y <= 1; ++y)
            {
                vec3 color = textureOffset(_TextureCurrent, uv, ivec2(x, y)).rgb;
                colorMin = min(colorMin, color);
                colorMax = max(colorMax, color);
            }
        }
        // colorHistory = vec4(clamp(colorHistory.rgb, colorMin, colorMax), 1.0);
    }
    else
    {
        // No color clamp.
    }

    // Current and history sample weight.
    float sourceWeight = sourceWeight_;
    float historyWeight = 1.0 - sourceWeight;

    // Luminance weighing.
    // if (luminanceWeigh > 0)
    if (colorClampingType != 0)
    {
        vec4 compressedSource
            = colorCurrent * rcp(max(max(colorCurrent.r, colorCurrent.g), colorCurrent.b) + 1.0);
        vec4 compressedHistory
            = colorHistory * rcp(max(max(colorHistory.r, colorHistory.g), colorHistory.b) + 1.0);

        float luminanceSource = Luminance(compressedSource.xyz);
        float luminanceHistory = Luminance(compressedHistory.xyz);

        // if (luminanceWeigh == 2)
        // {
        //     luminanceSource = texture(_TextureLuminanceCurrent, vec2(0.5, 0.5)).x;
        //     luminanceHistory = texture(_TextureLuminanceHistory, vec2(0.5, 0.5)).x;
        // }

        sourceWeight *= 1.0 / (1.0 + luminanceSource);
        historyWeight *= 1.0 / (1.0 + luminanceHistory);
    }

    vec3 result;

    if (colorClampingType != 0)
    {
        colorHistory = vec4(clamp(colorHistory.rgb, colorMin, colorMax), 1.0);

        result = ((colorCurrent * sourceWeight + colorHistory * historyWeight)
                  / max(sourceWeight + historyWeight, 0.00001))
                     .rgb;

        out_FragColor = vec4(result, 1.0);
    }
    else
    {
        out_FragColor = vec4(mix(colorCurrent, colorHistory, historyWeight).rgb, 1.0);
    }

    // Anti-flickering, based on Brian Karis talk @Siggraph 2014
    // https://de45xmedrsdbp.cloudfront.net/Resources/files/TemporalAA_small-59732822.pdf
    // Reduce blend factor when history is near clamping
    if (colorClampingType == 3)
    {
        float distToClamp = min(abs(colorMin.x - colorHistory.x), abs(colorMax.x - colorHistory.x));

        float gAlpha = 0.5;
        float alpha
            = clamp((gAlpha * distToClamp) / (distToClamp + colorMax.x - colorMin.x), 0.f, 1.f);

        colorHistory = vec4(clamp(colorHistory.rgb, colorMin, colorMax), 1.0);
        // float3 result = YCgCoToRGB(lerp(history, color, alpha));

        result = mix(colorHistory.rgb, colorCurrent.rgb, alpha);

        out_FragColor = vec4(result, 1.f);
    }
}