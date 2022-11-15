#version 460 core

#include "Shaders/Include/TransparentFragment.inc.glsl"

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 out_FragColor;

layout(binding = 0) uniform sampler2D _TextureScene;

layout(binding = 0, r32ui) uniform uimage2D _Heads;

layout(std430, binding = 3) buffer Lists
{
    TransparentFragment _Fragments[];
};

#define MAX_FRAGMENTS 64

void main()
{
    TransparentFragment frags[MAX_FRAGMENTS];

    int numFragments = 0;
    uint idx = imageLoad(_Heads, ivec2(gl_FragCoord.xy)).r;

    // Copy fragments linked list to array.
    while (idx != 0xFFFFFFFF && numFragments < MAX_FRAGMENTS)
    {
        frags[numFragments] = _Fragments[idx];
        numFragments++;
        idx = _Fragments[idx].next;
    }

    // Sort array by depth (insertion sort is used here).
    for (int i = 1; i < numFragments; i++)
    {
        TransparentFragment toInsert = frags[i];
        uint j = i;
        while (j > 0 && toInsert.depth > frags[j - 1].depth)
        {
            frags[j] = frags[j - 1];
            j--;
        }
        frags[j] = toInsert;
    }

    // Get color of closest non-transparent object from the frame buffer.
    vec4 color = texture(_TextureScene, uv);

    // Combine color alpha channels.
    for (int i = 0; i < numFragments; i++)
    {
        color = mix(color, vec4(frags[i].color), clamp(float(frags[i].color.a), 0.0, 1.0));
    }

    out_FragColor = color;
}
