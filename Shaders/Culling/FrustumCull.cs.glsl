
#version 460 core

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

#include "Shaders/Include/SceneData.inc.glsl"

struct AABB
{
    float pt[6];
};

layout(std430, binding = 1) buffer BoundingBoxes
{
    AABB _AABBs[];
};

struct DrawCommand
{
    uint count;
    uint instanceCount;
    uint firstIndex;
    uint baseVertex;
    uint baseInstance;
};

layout(std430, binding = 2) buffer DrawCommands
{
    DrawCommand _DrawCommands[];
};

layout(std430, binding = 3) buffer NumVisibleMeshes
{
    uint _NumVisibleMeshes;
};

#define Box_min_x box.pt[0]
#define Box_min_y box.pt[1]
#define Box_min_z box.pt[2]
#define Box_max_x box.pt[3]
#define Box_max_y box.pt[4]
#define Box_max_z box.pt[5]

bool isAABBinFrustum(AABB box)
{
    for (int i = 0; i < 6; i++)
    {
        int r = 0;
        r += (dot(frustumPlanes[i], vec4(Box_min_x, Box_min_y, Box_min_z, 1.0f)) < 0.0) ? 1 : 0;
        r += (dot(frustumPlanes[i], vec4(Box_max_x, Box_min_y, Box_min_z, 1.0f)) < 0.0) ? 1 : 0;
        r += (dot(frustumPlanes[i], vec4(Box_min_x, Box_max_y, Box_min_z, 1.0f)) < 0.0) ? 1 : 0;
        r += (dot(frustumPlanes[i], vec4(Box_max_x, Box_max_y, Box_min_z, 1.0f)) < 0.0) ? 1 : 0;
        r += (dot(frustumPlanes[i], vec4(Box_min_x, Box_min_y, Box_max_z, 1.0f)) < 0.0) ? 1 : 0;
        r += (dot(frustumPlanes[i], vec4(Box_max_x, Box_min_y, Box_max_z, 1.0f)) < 0.0) ? 1 : 0;
        r += (dot(frustumPlanes[i], vec4(Box_min_x, Box_max_y, Box_max_z, 1.0f)) < 0.0) ? 1 : 0;
        r += (dot(frustumPlanes[i], vec4(Box_max_x, Box_max_y, Box_max_z, 1.0f)) < 0.0) ? 1 : 0;
        if (r == 8)
            return false;
    }

    int r = 0;
    r = 0;
    for (int i = 0; i < 8; i++)
        r += ((frustumCorners[i].x > Box_max_x) ? 1 : 0);
    if (r == 8)
        return false;
    r = 0;
    for (int i = 0; i < 8; i++)
        r += ((frustumCorners[i].x < Box_min_x) ? 1 : 0);
    if (r == 8)
        return false;
    r = 0;
    for (int i = 0; i < 8; i++)
        r += ((frustumCorners[i].y > Box_max_y) ? 1 : 0);
    if (r == 8)
        return false;
    r = 0;
    for (int i = 0; i < 8; i++)
        r += ((frustumCorners[i].y < Box_min_y) ? 1 : 0);
    if (r == 8)
        return false;
    r = 0;
    for (int i = 0; i < 8; i++)
        r += ((frustumCorners[i].z > Box_max_z) ? 1 : 0);
    if (r == 8)
        return false;
    r = 0;
    for (int i = 0; i < 8; i++)
        r += ((frustumCorners[i].z < Box_min_z) ? 1 : 0);
    if (r == 8)
        return false;

    return true;
}

void main()
{
    const uint idx = gl_GlobalInvocationID.x;

    if (idx < numShapesToCull)
    {
        AABB box = _AABBs[_DrawCommands[idx].baseInstance >> 16];
        uint numInstances = isAABBinFrustum(box) ? 1 : 0;
        _DrawCommands[idx].instanceCount = numInstances;
        atomicAdd(_NumVisibleMeshes, numInstances);
    }
    else
    {
        _DrawCommands[idx].instanceCount = 1;
    }
}
