#version 460 core

#include "Shaders/Include/SceneData.inc.glsl"
#include "Shaders/Include/TAAFrameData.inc.glsl"

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_FragColor;

layout(binding = 0) uniform sampler2D _TextureCurrent;
layout(binding = 1) uniform sampler2D _TextureHistory;

layout(binding = 2) uniform sampler2D _TextureVelocity;

void main()
{
    // vec2 velocity = texture(_TextureVelocity, uv).xy;
    // vec2 previousPixelPos = uv - velocity;
    // vec4 colorCurrent = texture(_TextureCurrent, uv);
    // vec4 colorHistory = texture(_TextureHistory, previousPixelPos);

    // // Color clamping.
    // vec3 minColor = vec3(10000.0);
    // vec3 maxColor = vec3(-10000.0);

    // for (int x = -1; x <= 1; ++x)
    // {
    //     for (int y = -1; y <= 1; ++y)
    //     {
    //         vec3 color = textureOffset(_TextureCurrent, uv, ivec2(x, y)).rgb;
    //         minColor = min(minColor, color);
    //         maxColor = max(maxColor, color);
    //     }
    // }

    // colorHistory = vec4(clamp(colorHistory.rgb, minColor, maxColor), 1.0);

    // // Luminance weighing.

    // float modulationFactor = 0.90;

    // out_FragColor = vec4(mix(colorCurrent, colorHistory, modulationFactor).rgb, 1.0);
    vec3 c;
    vec3 neighbourhood[9];

    vec2 uPixelSize = vec2(1.0 / 1920, 1.0 / 1080);

    neighbourhood[0] = texture2D(_TextureCurrent, uv.xy + vec2(-1, -1) * uPixelSize).xyz;
    neighbourhood[1] = texture2D(_TextureCurrent, uv.xy + vec2(+0, -1) * uPixelSize).xyz;
    neighbourhood[2] = texture2D(_TextureCurrent, uv.xy + vec2(+1, -1) * uPixelSize).xyz;
    neighbourhood[3] = texture2D(_TextureCurrent, uv.xy + vec2(-1, +0) * uPixelSize).xyz;
    neighbourhood[4] = texture2D(_TextureCurrent, uv.xy + vec2(+0, +0) * uPixelSize).xyz;
    neighbourhood[5] = texture2D(_TextureCurrent, uv.xy + vec2(+1, +0) * uPixelSize).xyz;
    neighbourhood[6] = texture2D(_TextureCurrent, uv.xy + vec2(-1, +1) * uPixelSize).xyz;
    neighbourhood[7] = texture2D(_TextureCurrent, uv.xy + vec2(+0, +1) * uPixelSize).xyz;
    neighbourhood[8] = texture2D(_TextureCurrent, uv.xy + vec2(+1, +1) * uPixelSize).xyz;

    vec3 nmin = neighbourhood[0];
    vec3 nmax = neighbourhood[0];
    for (int i = 1; i < 9; ++i)
    {
        nmin = min(nmin, neighbourhood[i]);
        nmax = max(nmax, neighbourhood[i]);
    }

    vec2 vel = texture2D(_TextureVelocity, uv.xy).xy;
    vec2 histUv = uv.xy - vel.xy;

    // sample from history buffer, with neighbourhood clamping.
    vec3 histSample = clamp(texture2D(_TextureHistory, histUv).xyz, nmin, nmax);

    // blend factor
    float blend = 0.05;

    bvec2 a = greaterThan(histUv, vec2(1.0, 1.0));
    bvec2 b = lessThan(histUv, vec2(0.0, 0.0));
    // if history sample is outside screen, switch to aliased image as a fallback.
    blend = any(bvec2(any(a), any(b))) ? 1.0 : blend;

    vec3 curSample = neighbourhood[4];
    // finally, blend current and clamped history sample.
    c = mix(histSample, curSample, vec3(blend));

    out_FragColor = vec4(c.xyz, 1.0);
}