#pragma once

#include <Core/Types.h>

#include <unordered_map>
#include <vector>

namespace Nerine
{

constexpr auto MAX_SCENE_LEVEL = 16;

struct SceneHierarchy
{
    u32 parent{u32(-1)};
    u32 firstChild{u32(-1)};
    u32 nextSibling{u32(-1)};
    u32 lastSibling{u32(-1)};
    u32 level{0};
};

// XXX: Use handle/strong typed ints for scene/node indices.
struct Scene
{
    std::vector<mat4> localTransforms;
    std::vector<mat4> globalTransforms;

    std::vector<SceneHierarchy> hierarchy;

    std::unordered_map<u32, u32> meshesMap;
    std::unordered_map<u32, u32> materialsMap;
    std::unordered_map<u32, u32> namesMap;

    std::vector<std::string> names;
    std::vector<std::string> materialNames;

    std::vector<int> changedAtThisFrame[MAX_SCENE_LEVEL];
};

inline std::string GetNodeName(const Scene& scene, u32 node)
{
    u32 id = scene.namesMap.contains(node) ? scene.namesMap.at(node) : u32(-1);
    return (id > u32(-1)) ? scene.names[id] : std::string();
}

inline void SetNodeName(Scene& scene, u32 node, const std::string& name)
{
    u32 id = (u32)scene.names.size();
    scene.names.push_back(name);
    scene.namesMap[node] = id;
}

u32 AddNode(Scene& scene, u32 parent, u32 level);
void RecalculateGlobalTransforms(Scene& scene);

void MergeScenes(Scene& scene, const std::vector<Scene*>& scenes,
                 const std::vector<mat4>& rootTransforms, const std::vector<u32>& meshCounts,
                 bool mergeMeshes = true, bool mergeMaterials = true);
void DeleteSceneNodes(Scene& scene, const std::vector<u32>& nodesToDelete);

void MarkAsChanged(Scene& scene, u32 node);
u32 FindNodeByName(const Scene& scene, const std::string& name);
u32 GetNodeLevel(const Scene& scene, u32 node);

bool SaveScene(const std::string& fileName, Scene& scene);
bool LoadScene(const std::string& fileName, Scene& scene);

} // namespace Nerine
