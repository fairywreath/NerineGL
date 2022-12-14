
#version 460 core

#include "Shaders/Include/SceneData.inc.glsl"
#include "Shaders/Include/TAAFrameData.inc.glsl"

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_FragColor;

layout(binding = 0) uniform sampler2D currentColorTex;
layout(binding = 1) uniform sampler2D previousColorTex;

layout(binding = 2) uniform sampler2D velocityTex;

layout(binding = 3) uniform sampler2D currentDepthTex;
layout(binding = 4) uniform sampler2D previousDepthTex;

// TAA settings
const float velocityScale = 0.9;
const float feedbackFactor = 1.0;
const float maxDepthFalloff = 1.0;

const vec2 resolution = vec2(1920, 1080);

const vec2 kOffsets3x3[9] = {
    vec2(-1, -1), // upper left
    vec2(0, -1),  // up
    vec2(1, -1),  // upper right
    vec2(-1, 0),  // left
    vec2(0, 0),   // K
    vec2(1, 0),   // right
    vec2(-1, 1),  // lower left
    vec2(0, 1),   // down
    vec2(1, 1),   // lower right
};                // k is index 4

// Number of neighbors.
const uint kNeighborsCount = 9;

// we can cut this down to 4
const vec2 kOffsets2x2[5] = {
    vec2(-1, 0), // left
    vec2(0, -1), // up
    vec2(0, 0),  // K
    vec2(1, 0),  // right
    vec2(0, 1)   // down
};               // k is index 3

const uint neighborCount = 5;

vec2 GetClosestUV(in sampler2D depths)
{
    vec2 deltaRes = vec2(1.0 / resolution.x, 1.0 / resolution.y);
    vec2 closestUV = uv;
    float closestDepth = 1.0f;

    for (uint iter = 0; iter < kNeighborsCount; iter++)
    {
        vec2 newUV = uv + (kOffsets3x3[iter] * deltaRes);

        float depth = texture2D(depths, newUV).x;

        if (depth < closestDepth)
        {
            closestDepth = depth;
            closestUV = newUV;
        }
    }

    return closestUV;
}

vec2 MinMaxDepths(in float neighborDepths[kNeighborsCount])
{
    float minDepth = neighborDepths[0];
    float maxDepth = neighborDepths[0];

    for (int iter = 1; iter < kNeighborsCount; iter++)
    {
        minDepth = min(minDepth, neighborDepths[iter]);
        minDepth = max(maxDepth, neighborDepths[iter]);
    }

    return vec2(minDepth, maxDepth);
}

vec4 MinColors(in vec4 neighborColors[neighborCount])
{
    vec4 minColor = neighborColors[0];

    for (int iter = 1; iter < neighborCount; iter++)
    {
        minColor = min(minColor, neighborColors[iter]);
    }

    return minColor;
}

vec4 MaxColors(in vec4 neighborColors[neighborCount])
{
    vec4 maxColor = neighborColors[0];

    for (int iter = 1; iter < neighborCount; iter++)
    {
        maxColor = max(maxColor, neighborColors[iter]);
    }

    return maxColor;
}

vec4 MinColors2(in vec4 neighborColors[kNeighborsCount])
{
    vec4 minColor = neighborColors[0];

    for (int iter = 1; iter < neighborCount; iter++)
    {
        minColor = min(minColor, neighborColors[iter]);
    }

    return minColor;
}

vec4 MaxColors2(in vec4 neighborColors[kNeighborsCount])
{
    vec4 maxColor = neighborColors[0];

    for (int iter = 1; iter < neighborCount; iter++)
    {
        maxColor = max(maxColor, neighborColors[iter]);
    }

    return maxColor;
}

vec4 ConstrainHistory(vec4 neighborColors[neighborCount])
{
    vec4 previousColorMin = MinColors(neighborColors);
    vec4 previousColorMax = MaxColors(neighborColors);

    // vec4 constrainedHistory = vec4(0);
    return clamp(neighborColors[2], previousColorMin, previousColorMax);
}

// note: clips towards aabb center + p.w
vec4 clip_aabb(vec3 colorMin, vec3 colorMax, vec4 currentColor, vec4 previousColor)
{
    vec3 p_clip = 0.5 * (colorMax + colorMin);
    vec3 e_clip = 0.5 * (colorMax - colorMin);
    vec4 v_clip = previousColor - vec4(p_clip, currentColor.a);
    vec3 v_unit = v_clip.rgb / e_clip;
    vec3 a_unit = abs(v_unit);
    float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));
    if (ma_unit > 1.0)
    {
        return vec4(p_clip, currentColor.a) + v_clip / ma_unit;
    }
    else
    {
        return previousColor; // point inside aabb
    }
}

vec4 Inside2Resolve(sampler2D currColorTex, sampler2D prevColorTex, vec2 velocity)
{
    vec2 deltaRes = vec2(1.0 / resolution.x, 1.0 / resolution.y);

    vec4 current3x3Colors[kNeighborsCount];
    vec4 previous3x3Colors[kNeighborsCount];

    for (uint iter = 0; iter < kNeighborsCount; iter++)
    {
        vec2 newUV = uv + (kOffsets3x3[iter] * deltaRes);

        current3x3Colors[iter] = texture2D(currColorTex, newUV);

        previous3x3Colors[iter] = texture2D(prevColorTex, newUV + velocity);
    }

    vec4 rounded3x3Min = MinColors2(current3x3Colors);
    vec4 rounded3x3Max = MaxColors2(previous3x3Colors);

    ///////////////////////////////////////////////////////////////////////////////
    vec4 current2x2Colors[neighborCount];
    vec4 previous2x2Colors[neighborCount];
    for (uint iter = 0; iter < neighborCount; iter++)
    {
        vec2 newUV = uv + (kOffsets2x2[iter] * deltaRes);

        current2x2Colors[iter] = texture2D(currColorTex, newUV);

        previous2x2Colors[iter] = texture2D(prevColorTex, newUV + velocity);
    }

    vec4 min2 = MinColors(current2x2Colors);
    vec4 max2 = MaxColors(previous2x2Colors);

    // mix the 3x3 and 2x2 min maxes together
    vec4 mixedMin = mix(rounded3x3Min, min2, 0.5);
    vec4 mixedMax = mix(rounded3x3Max, max2, 0.5);

    float testVel = feedbackFactor - (length(velocity) * velocityScale);
    return mix(current2x2Colors[2],
               clip_aabb(mixedMin.rgb, mixedMax.rgb, current2x2Colors[2],
                         ConstrainHistory(previous2x2Colors)),
               testVel);
}

vec4 Custom2Resolve(in float preNeighborDepths[kNeighborsCount],
                    in float curNeighborDepths[kNeighborsCount], vec2 velocity)
{
    // use the closest depth instead?
    vec2 preMinMaxDepths = MinMaxDepths(preNeighborDepths);
    vec2 curMinMaxDepths = MinMaxDepths(curNeighborDepths);

    float highestDepth = min(preMinMaxDepths.x, curMinMaxDepths.x); // get the furthest
    float lowestDepth = max(preMinMaxDepths.x, curMinMaxDepths.x);  // get the closest

    float depthFalloff = abs(highestDepth - lowestDepth);

    vec4 res = vec4(0);

    // float testVel = feedbackFactor - (length(velocity) * velocityScale);
    vec4 taa = Inside2Resolve(currentColorTex, previousColorTex, velocity);

    return taa;

    float averageDepth = 0;
    for (uint iter = 0; iter < kNeighborsCount; iter++)
    {
        averageDepth += curNeighborDepths[iter];
    }

    averageDepth /= kNeighborsCount;

    // for dithered edges, detect if the adge has been dithered?
    // use a 3x3 grid to see if anyhting around it has high enough depth?
    if (averageDepth < maxDepthFalloff)
    {
        res = taa; // vec4(1, 0, 0, 1);
    }

    else
    {
        res = texture2D(currentColorTex, uv);
        ;
    }

    return res;
}

void main()
{
    float currentDepths[kNeighborsCount];
    float previousDepths[kNeighborsCount];

    vec2 deltaRes = vec2(1.0 / resolution.x, 1.0 / resolution.y);

    // vec2 closestVec = -texture2D(velocityTex, GetClosestUV(currentDepthTex)).rg;
    vec2 closestVec = -texture2D(velocityTex, uv).rg;

    for (uint iter = 0; iter < kNeighborsCount; iter++)
    {
        vec2 newUV = uv + (kOffsets3x3[iter] * deltaRes);

        currentDepths[iter] = texture2D(currentDepthTex, newUV).x;
        previousDepths[iter] = texture2D(previousDepthTex, newUV + closestVec).x;
    }

    // out_FragColor = Custom2Resolve(previousDepths, currentDepths, closestVec);

    // out_FragColor = texture2D(currentColorTex, uv);

    vec4 taa = Inside2Resolve(currentColorTex, previousColorTex, closestVec);

    out_FragColor = taa;
}