#version 460 core

#include "Shaders/Include/SceneData.inc.glsl"
#include "Shaders/Include/TAAFrameData.inc.glsl"

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_FragColor;

layout(binding = 0) uniform sampler2D _TextureCurrent;
layout(binding = 1) uniform sampler2D _TextureHistory;
layout(binding = 2) uniform sampler2D _TextureVelocity;

layout(binding = 3) uniform sampler2D _TextureDepth;

struct TAAParams
{
    // Clean history by clamping.
    bool cleanHistory;

    float modulationFactor;
};

// https://www.shadertoy.com/view/3tjyWy.
float Mitchell(float x)
{
    const float B = 1.0;
    const float C = 0.0;

    x = abs(x);

    float a = x < 1.0 ? 1.0 - 2.0 / 6.0 * B : 8.0 / 6.0 * B + 4.0 * C;
    float b = x < 1.0 ? 0.0 : -2.0 * B - 8.0 * C;
    float c = x < 1.0 ? -3.0 + 2.0 * B + 1.0 * C : 1.0 * B + 5.0 * C;
    float d = x < 1.0 ? 2.0 - 1.5 * B - 1.0 * C : -1.0 / 6.0 * B - 1.0 * C;

    return a + x * (b + x * (c + x * d));
}

// Samples a texture with Catmull - Rom filtering, using 9 texture fetches instead of 16.
// See http://vec3.ca/bicubic-filtering-in-fewer-taps/ for more details
vec4 SampleTextureCatmullRom(sampler2D tex,
                             // Need GL sampler type from CPU for this,
                             // in SamplerState linearSampler,
                             vec2 uv, vec2 texSize)
{
    // We're going to sample a a 4x4 grid of texels surrounding the target UV coordinate. We'll do
    // this by rounding down the sample location to get the exact center of our "starting" texel.
    // The starting texel will be at location [1, 1] in the grid, where [0, 0] is the top left
    // corner.
    vec2 samplePos = uv * texSize;
    vec2 texPos1 = floor(samplePos - 0.5f) + 0.5f;

    // Compute the fractional offset from our starting texel to our original sample location, which
    // we'll feed into the Catmull-Rom spline function to get our filter weights.
    vec2 f = samplePos - texPos1;

    // Compute the Catmull-Rom weights using the fractional offset that we calculated earlier.
    // These equations are pre-expanded based on our knowledge of where the texels will be located,
    // which lets us avoid having to evaluate a piece-wise function.
    vec2 w0 = f * (-0.5f + f * (1.0f - 0.5f * f));
    vec2 w1 = 1.0f + f * f * (-2.5f + 1.5f * f);
    vec2 w2 = f * (0.5f + f * (2.0f - 1.5f * f));
    vec2 w3 = f * f * (-0.5f + 0.5f * f);

    // Work out weighting factors and sampling offsets that will let us use bilinear filtering to
    // simultaneously evaluate the middle 2 samples from the 4x4 grid.
    vec2 w12 = w1 + w2;
    vec2 offset12 = w2 / (w1 + w2);

    // Compute the final UV coordinates we'll use for sampling the texture
    vec2 texPos0 = texPos1 - 1;
    vec2 texPos3 = texPos1 + 2;
    vec2 texPos12 = texPos1 + offset12;

    texPos0 /= texSize;
    texPos3 /= texSize;
    texPos12 /= texSize;

    vec4 result = vec4(0.0, 0.0, 0.0, 0.0);

    result += texture(tex, vec2(texPos0.x, texPos0.y)) * w0.x * w0.y;
    result += texture(tex, vec2(texPos12.x, texPos0.y)) * w12.x * w0.y;
    result += texture(tex, vec2(texPos3.x, texPos0.y)) * w3.x * w0.y;

    result += texture(tex, vec2(texPos0.x, texPos12.y)) * w0.x * w12.y;
    result += texture(tex, vec2(texPos12.x, texPos12.y)) * w12.x * w12.y;
    result += texture(tex, vec2(texPos3.x, texPos12.y)) * w3.x * w12.y;

    result += texture(tex, vec2(texPos0.x, texPos3.y)) * w0.x * w3.y;
    result += texture(tex, vec2(texPos12.x, texPos3.y)) * w12.x * w3.y;
    result += texture(tex, vec2(texPos3.x, texPos3.y)) * w3.x * w3.y;

    // result += tex.SampleLevel(linearSampler, vec2(texPos0.x, texPos0.y), 0.0f) * w0.x * w0.y;
    // result += tex.SampleLevel(linearSampler, vec2(texPos12.x, texPos0.y), 0.0f) * w12.x * w0.y;
    // result += tex.SampleLevel(linearSampler, vec2(texPos3.x, texPos0.y), 0.0f) * w3.x * w0.y;

    // result += tex.SampleLevel(linearSampler, vec2(texPos0.x, texPos12.y), 0.0f) * w0.x * w12.y;
    // result += tex.SampleLevel(linearSampler, vec2(texPos12.x, texPos12.y), 0.0f) * w12.x * w12.y;
    // result += tex.SampleLevel(linearSampler, vec2(texPos3.x, texPos12.y), 0.0f) * w3.x * w12.y;

    // result += tex.SampleLevel(linearSampler, vec2(texPos0.x, texPos3.y), 0.0f) * w0.x * w3.y;
    // result += tex.SampleLevel(linearSampler, vec2(texPos12.x, texPos3.y), 0.0f) * w12.x * w3.y;
    // result += tex.SampleLevel(linearSampler, vec2(texPos3.x, texPos3.y), 0.0f) * w3.x * w3.y;

    return result;
}

// https://github.com/playdeadgames/temporal/blob/master/Assets/Shaders/TemporalReprojection.shader
// AUTHOR: Lasse Jon Fuglsang Pedersen <lasse@playdead.com>
vec4 clipAABB(vec3 aabb_min, vec3 aabb_max, vec4 p, vec4 q)
{
    const float FLT_EPS = 0.0001f;

    // note: only clips towards aabb center (but fast!)
    vec3 p_clip = 0.5 * (aabb_max + aabb_min);
    vec3 e_clip = 0.5 * (aabb_max - aabb_min) + FLT_EPS;

    vec4 v_clip = q - vec4(p_clip, p.w);
    vec3 v_unit = v_clip.xyz / e_clip;
    vec3 a_unit = abs(v_unit);
    float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));

    if (ma_unit > 1.0)
        return vec4(p_clip, p.w) + v_clip / ma_unit;
    else
        return q; // point inside aabb
}

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

    // Cleaning on history color.
    vec4 nearColor0 = textureOffset(_TextureCurrent, uv, ivec2(1, 0));
    vec4 nearColor1 = textureOffset(_TextureCurrent, uv, ivec2(0, 1));
    vec4 nearColor2 = textureOffset(_TextureCurrent, uv, ivec2(-1, 0));
    vec4 nearColor3 = textureOffset(_TextureCurrent, uv, ivec2(0, -1));

    vec4 boxMin = min(colorCurrent, min(nearColor0, min(nearColor1, min(nearColor2, nearColor3))));
    vec4 boxMax = max(colorCurrent, max(nearColor0, max(nearColor1, max(nearColor2, nearColor3))));

    colorHistory = clamp(colorHistory, boxMin, boxMax);

    // float modulationFactor = 0.92;

    float sourceWeight = 0.05;
    float historyWeight = 1.0 - sourceWeight;
    vec4 compressedSource
        = colorCurrent * rcp(max(max(colorCurrent.r, colorCurrent.g), colorCurrent.b) + 1.0);
    vec4 compressedHistory
        = colorHistory * rcp(max(max(colorHistory.r, colorHistory.g), colorHistory.b) + 1.0);
    float luminanceSource = Luminance(compressedSource.xyz);
    float luminanceHistory = Luminance(compressedHistory.xyz);
    sourceWeight *= 1.0 / (1.0 + luminanceSource);
    historyWeight *= 1.0 / (1.0 + luminanceHistory);

    vec3 result = ((colorCurrent * sourceWeight + colorHistory * historyWeight)
                   / max(sourceWeight + historyWeight, 0.00001))
                      .xyz;

    out_FragColor = vec4(result, 1.0);

    // out_FragColor = vec4(mix(colorCurrent, colorHistory, modulationFactor).rgb, 1.0);

    // return;

    // More advance from Alex Tardiff.
    // vec3 sourceSampleTotal = vec3(0, 0, 0);
    // float sourceSampleWeight = 0.0;
    // vec3 neighborhoodMin = vec3(10000);
    // vec3 neighborhoodMax = vec3(-10000);
    // vec3 m1 = vec3(0, 0, 0);
    // vec3 m2 = vec3(0, 0, 0);
    // float closestDepth = 0.0;
    // ivec2 closestDepthPixelPosition = ivec2(0, 0);

    // // Neighborhood sampling.
    // for (int x = -1; x <= 1; x++)
    // {
    //     for (int y = -1; y <= 1; y++)
    //     {
    //         // XXX: Use gl_FragCoord instead of uv here? Since we are sampling pixel position.
    //         //  - Does UV directly corelate with pixelpos(it should since we are rendering a
    //         quad)?
    //         //  - Does vec2->ivec2 cast automatically round nicely?
    //         //  - Which sampler to use here?
    //         ivec2 pixelPosition = ivec2(uv.xy) + ivec2(x, y);
    //         pixelPosition = clamp(pixelPosition, ivec2(0, 0), currViewSize - 1);

    //         // Current color.
    //         vec3 neighbor = max(vec3(0, 0, 0), texture(_TextureCurrent, uv).rgb);

    //         float subSampleDistance = length(vec2(x, y));
    //         float subSampleWeight = Mitchell(subSampleDistance);

    //         sourceSampleTotal += neighbor * subSampleWeight;
    //         sourceSampleWeight += subSampleWeight;

    //         neighborhoodMin = min(neighborhoodMin, neighbor);
    //         neighborhoodMax = max(neighborhoodMax, neighbor);

    //         m1 += neighbor;
    //         m2 += neighbor * neighbor;

    //         float currentDepth = texture(_TextureDepth, uv).r;

    //         if (currentDepth > closestDepth)
    //         {
    //             closestDepth = currentDepth;
    //             closestDepthPixelPosition = pixelPosition;
    //         }
    //     }
    // }

    // // vec2 motionVector = VelocityBuffer[closestDepthPixelPosition].xy * vec2(0.5, -0.5);
    // vec2 motionVector = texture(_TextureVelocity, closestDepthPixelPosition).xy * vec2(0.5,
    // -0.5);
    // // vec2 motionVector = texture(_TextureVelocity, closestDepthPixelPosition).xy;
    // // vec2 historyTexCoord = i.texCoord.xy - motionVector;
    // vec2 historyTexCoord = uv - motionVector;
    // vec3 sourceSample = sourceSampleTotal / sourceSampleWeight;
    // if (any(historyTexCoord != clamp(historyTexCoord, vec2(0, 0), vec2(1, 1))))
    // {
    //     // return vec4(sourceSample, 1);
    //     return;
    // }

    // // vec3 historySample = SampleTextureCatmullRom(_TextureHistory, /* linearSampler ,*/
    // //  historyTexCoord, vec2(currViewSize.xy))
    // //  .rgb;
    // vec3 historySample = texture(_TextureHistory, historyTexCoord).rgb;

    // // Variance clipping.
    // float oneDividedBySampleCount = 1.0 / 9.0;
    // float gamma = 1.0;
    // vec3 mu = m1 * oneDividedBySampleCount;
    // vec3 sigma = sqrt(abs((m2 * oneDividedBySampleCount) - (mu * mu)));
    // vec3 minc = mu - gamma * sigma;
    // vec3 maxc = mu + gamma * sigma;

    // // Need to get current average sample here.
    // // historySample = clipAABB(minc, maxc, vec4(0, 0, 0, 0),
    // //  vec4(clamp(historySample, neighborhoodMin, neighborhoodMax).xyz, 1.0))
    // // .xyz;

    // // Cleaning on history color.
    // vec4 nearColor0 = textureOffset(_TextureCurrent, uv, ivec2(1, 0));
    // vec4 nearColor1 = textureOffset(_TextureCurrent, uv, ivec2(0, 1));
    // vec4 nearColor2 = textureOffset(_TextureCurrent, uv, ivec2(-1, 0));
    // vec4 nearColor3 = textureOffset(_TextureCurrent, uv, ivec2(0, -1));

    // vec4 boxMin = min(minc, min(nearColor0, min(nearColor1, min(nearColor2, nearColor3))));
    // vec4 boxMax = max(maxc, max(nearColor0, max(nearColor1, max(nearColor2, nearColor3))));

    // historySample = clamp(vec4(historySample.xyz, 1.0), boxMin, boxMax).xyz;

    // float sourceWeight = 0.05;
    // float historyWeight = 1.0 - sourceWeight;
    // vec3 compressedSource
    //     = sourceSample * rcp(max(max(sourceSample.r, sourceSample.g), sourceSample.b) + 1.0);
    // vec3 compressedHistory
    //     = historySample * rcp(max(max(historySample.r, historySample.g), historySample.b) + 1.0);
    // float luminanceSource = Luminance(compressedSource);
    // float luminanceHistory = Luminance(compressedHistory);
    // sourceWeight *= 1.0 / (1.0 + luminanceSource);
    // historyWeight *= 1.0 / (1.0 + luminanceHistory);
    // vec3 result = (sourceSample * sourceWeight + historySample * historyWeight)
    //               / max(sourceWeight + historyWeight, 0.00001);

    // out_FragColor = vec4(result, 1.0);
}
