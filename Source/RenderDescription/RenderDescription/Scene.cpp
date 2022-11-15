#include "Scene.h"
#include "Utils.h"

#include <Core/Logger.h>
#include <Core/Utils.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <numeric>

namespace fs = std::filesystem;

namespace Nerine
{

u32 AddNode(Scene& scene, u32 parent, u32 level)
{
    u32 node = (u32)scene.hierarchy.size();
    scene.localTransforms.push_back(glm::mat4(1.0f));
    scene.globalTransforms.push_back(glm::mat4(1.0f));

    scene.hierarchy.push_back({
        .parent = parent,
        .lastSibling = u32(-1),
    });

    if (parent != u32(-1))
    {
        u32 firstChild = scene.hierarchy[parent].firstChild;

        if (firstChild == u32(-1))
        {
            scene.hierarchy[parent].firstChild = node;
            scene.hierarchy[node].lastSibling = node;
        }
        else
        {
            u32 lastSibling = scene.hierarchy[firstChild].lastSibling;
            if (lastSibling == u32(-1))
            {
                // No cached last sibling, iterate nextSibling to get last sibling.
                for (lastSibling = firstChild; scene.hierarchy[lastSibling].nextSibling != u32(-1);
                     lastSibling = scene.hierarchy[lastSibling].nextSibling)
                    ;
            }

            scene.hierarchy[lastSibling].nextSibling = node;
            scene.hierarchy[firstChild].lastSibling = node;
        }
    }

    scene.hierarchy[node].level = level;
    scene.hierarchy[node].nextSibling = u32(-1);
    scene.hierarchy[node].firstChild = u32(-1);

    return node;
}

void RecalculateGlobalTransforms(Scene& scene)
{
    // Update root node global transform.
    if (!scene.changedAtThisFrame[0].empty())
    {
        int node = scene.changedAtThisFrame[0][0];
        scene.globalTransforms[node] = scene.localTransforms[node];
        scene.changedAtThisFrame[0].clear();
    }

    for (unsigned int i = 1; i < MAX_SCENE_LEVEL && (!scene.changedAtThisFrame[i].empty()); i++)
    {
        for (const auto& node : scene.changedAtThisFrame[i])
        {
            auto parent = scene.hierarchy[node].parent;
            scene.globalTransforms[node]
                = scene.globalTransforms[parent] * scene.localTransforms[node];
        }
        scene.changedAtThisFrame[i].clear();
    }
}

void MarkAsChanged(Scene& scene, u32 node)
{
    u32 level = scene.hierarchy[node].level;
    scene.changedAtThisFrame[level].push_back(node);

    // XXX: Do this iteratively.
    for (auto s = scene.hierarchy[node].firstChild; s != u32(-1);
         s = scene.hierarchy[s].nextSibling)
        MarkAsChanged(scene, s);
}

u32 FindNodeByName(const Scene& scene, const std::string& name)
{
    // Linear search on each scene id.
    for (auto i = 0; i < scene.localTransforms.size(); i++)
    {
        if (scene.namesMap.contains(i))
        {
            u32 strID = scene.namesMap.at(i);
            if (strID != u32(-1))
                if (scene.names[strID] == name)
                    return (int)i;
        }
    }

    return u32(-1);
}

u32 GetNodeLevel(const Scene& scene, u32 node)
{
    u32 level = 0;

    u32 parent = scene.hierarchy[node].parent;
    while (parent != u32(-1))
    {
        parent = scene.hierarchy[parent].parent;
        level++;
    }

    return level;
}

/*
 * Scene saving and loading.
 */
bool SaveScene(const std::string& fileName, Scene& scene)
{
    std::ofstream file(fileName, std::ios::out | std::ios::binary);
    if (!file)
    {
        LOG_ERROR("saveScene: failed to open file ", fs::absolute(fileName));
        return false;
    }

    const u32 nodeCount = (u32)scene.hierarchy.size();
    file.write((const char*)&nodeCount, sizeof(nodeCount));

    file.write((const char*)scene.localTransforms.data(), sizeof(mat4) * nodeCount);
    file.write((const char*)scene.globalTransforms.data(), sizeof(mat4) * nodeCount);
    file.write((const char*)scene.hierarchy.data(), sizeof(SceneHierarchy) * nodeCount);

    SaveMap(file, scene.materialsMap);
    SaveMap(file, scene.meshesMap);

    if (!scene.names.empty() && !scene.namesMap.empty())
    {
        SaveMap(file, scene.namesMap);
        SaveStringArray(file, scene.names);
        SaveStringArray(file, scene.materialNames);
    }

    file.close();

    return true;
}

bool LoadScene(const std::string& fileName, Scene& scene)
{
    std::ifstream file(fileName, std::ios::out | std::ios::binary);

    if (!file)
    {
        LOG_ERROR("LoadScene: failed to open ", fs::absolute(fileName));
        return false;
    }

    u32 size = 0;
    file.read((char*)&size, sizeof(size));

    scene.hierarchy.resize(size);
    scene.globalTransforms.resize(size);
    scene.localTransforms.resize(size);

    /*
     * XXX:
     *   - Check that scene nodes are valid / not u32(-1)
     *   - Recalculate ChangedAtThisLevel / find max depth of a node, or cache max level/depth
     * somewhere.
     */

    file.read((char*)scene.localTransforms.data(), sizeof(mat4) * size);
    file.read((char*)scene.globalTransforms.data(), sizeof(mat4) * size);
    file.read((char*)scene.hierarchy.data(), sizeof(SceneHierarchy) * size);

    LoadMap(file, scene.materialsMap);
    LoadMap(file, scene.meshesMap);

    if (!file.eof())
    {
        LoadMap(file, scene.namesMap);
        LoadStringArray(file, scene.names);
        LoadStringArray(file, scene.materialNames);
    }

    if (!file.good())
    {
        LOG_ERROR("loadScene: failed to read scene data - ", fileName);
        return false;
    }

    file.close();

    return true;
}

/*
 * More advanced scene operations.
 */
namespace
{

/*
 * Shift hierarchies.
 */
void ShiftNodes(Scene& scene, u32 startOffset, u32 nodeCount, u32 shiftAmount)
{
    auto ShiftNode = [shiftAmount](SceneHierarchy& node) {
        if (node.parent != u32(-1))
            node.parent += shiftAmount;
        if (node.firstChild != u32(-1))
            node.firstChild += shiftAmount;
        if (node.nextSibling != u32(-1))
            node.nextSibling += shiftAmount;
        if (node.lastSibling != u32(-1))
            node.lastSibling += shiftAmount;
    };

    for (int i = 0; i < nodeCount; i++)
        ShiftNode(scene.hierarchy[i + startOffset]);
}

using ItemMap = std::unordered_map<u32, u32>;
void MergeMaps(ItemMap& m, const ItemMap& otherMap, int indexOffset, int itemOffset)
{
    for (const auto& i : otherMap)
        m[i.first + indexOffset] = i.second + itemOffset;
}

void AddUniqueIndex(std::vector<u32>& v, u32 index)
{
    if (!std::binary_search(v.begin(), v.end(), index))
        v.push_back(index);
}

// Recurse down the tree and add nodes to delete.
void CollectNodesToDelete(const Scene& scene, u32 node, std::vector<u32>& nodes)
{
    for (int n = scene.hierarchy[node].firstChild; n != u32(-1); n = scene.hierarchy[n].nextSibling)
    {
        AddUniqueIndex(nodes, n);
        CollectNodesToDelete(scene, n, nodes);
    }
}

u32 FindLastNonDeletedItem(const Scene& scene, const std::vector<u32>& newIndices, u32 node)
{
    if (node == u32(-1))
        return u32(-1);

    return (newIndices[node] == u32(-1))
               ? FindLastNonDeletedItem(scene, newIndices, scene.hierarchy[node].nextSibling)
               : newIndices[node];
}

// Set map values based on newIndices.
void ShiftMapIndices(std::unordered_map<u32, u32>& items, const std::vector<u32>& newIndices)
{
    std::unordered_map<u32, u32> newItems;
    for (const auto& m : items)
    {
        int newIndex = newIndices[m.first];
        if (newIndex != u32(-1))
            newItems[newIndex] = m.second;
    }
    items = newItems;
}

} // namespace

void MergeScenes(Scene& scene, const std::vector<Scene*>& scenes,
                 const std::vector<glm::mat4>& rootTransforms, const std::vector<u32>& meshCounts,
                 bool mergeMeshes, bool mergeMaterials)
{
    // New root node.
    scene.hierarchy = {{
        .parent = u32(-1),
        .firstChild = 1,
        .nextSibling = u32(-1),
        .lastSibling = u32(-1),
        .level = 0,
    }};

    scene.namesMap[0] = 0;
    scene.names = {"NewRootNode"};

    scene.localTransforms.push_back(glm::mat4(1.f));
    scene.globalTransforms.push_back(glm::mat4(1.f));

    if (scenes.empty())
        return;

    int offs = 1;
    int meshOffs = 0;
    int nameOffs = (int)scene.names.size();
    int materialOfs = 0;
    auto meshCount = meshCounts.begin();

    if (!mergeMaterials)
        scene.materialNames = scenes[0]->materialNames;

    for (const auto* s : scenes)
    {
        MergeVectors(scene.localTransforms, s->localTransforms);
        MergeVectors(scene.globalTransforms, s->globalTransforms);

        MergeVectors(scene.hierarchy, s->hierarchy);

        MergeVectors(scene.names, s->names);
        if (mergeMaterials)
            MergeVectors(scene.materialNames, s->materialNames);

        u32 nodeCount = (int)s->hierarchy.size();

        ShiftNodes(scene, offs, nodeCount, offs);

        MergeMaps(scene.meshesMap, s->meshesMap, offs, mergeMeshes ? meshOffs : 0);
        MergeMaps(scene.materialsMap, s->materialsMap, offs, mergeMaterials ? materialOfs : 0);
        MergeMaps(scene.namesMap, s->namesMap, offs, nameOffs);

        offs += nodeCount;

        materialOfs += (int)s->materialNames.size();
        nameOffs += (int)s->names.size();

        if (mergeMeshes)
        {
            meshOffs += *meshCount;
            meshCount++;
        }
    }

    offs = 1;
    int idx = 0;
    for (const auto* s : scenes)
    {
        int nodeCount = (int)s->hierarchy.size();
        bool isLast = (idx == scenes.size() - 1);

        int next = isLast ? u32(-1) : offs + nodeCount;
        scene.hierarchy[offs].nextSibling = next;

        scene.hierarchy[offs].parent = 0;

        if (!rootTransforms.empty())
            scene.localTransforms[offs] = rootTransforms[idx] * scene.localTransforms[offs];

        offs += nodeCount;
        idx++;
    }

    for (auto i = scene.hierarchy.begin() + 1; i != scene.hierarchy.end(); i++)
        i->level++;
}

/*
 * Approximately an O ( N * Log(N) * Log(M)) algorithm (N = scene.size, M = nodesToDelete.size) to
 * delete a collection of nodes. See:
 * https://github.com/PacktPublishing/3D-Graphics-Rendering-Cookbook/blob/master/shared/scene/Scene.cpp
 */
void DeleteSceneNodes(Scene& scene, const std::vector<u32>& nodesToDelete)
{
    // 0) Add all the nodes down below in the hierarchy
    auto indicesToDelete = nodesToDelete;
    for (auto i : indicesToDelete)
        CollectNodesToDelete(scene, i, indicesToDelete);

    // aux array with node indices to keep track of the moved ones [moved = [](node) { return (node
    // != nodes[node]); ]
    std::vector<u32> nodes(scene.hierarchy.size());
    std::iota(nodes.begin(), nodes.end(), 0);

    // 1.a) Move all the indicesToDelete to the end of 'nodes' array (and cut them off, a variation
    // of swap'n'pop for multiple elements)
    auto oldSize = nodes.size();
    EraseSelected(nodes, indicesToDelete);

    // 1.b) Make a newIndices[oldIndex] mapping table
    std::vector<u32> newIndices(oldSize, u32(-1));
    for (int i = 0; i < nodes.size(); i++)
        newIndices[nodes[i]] = i;

    // 2) Replace all non-null parent/firstChild/nextSibling pointers in all the nodes by new
    // positions
    auto nodeMover = [&scene, &newIndices](SceneHierarchy& h) {
        return SceneHierarchy{
            .parent = (h.parent != u32(-1)) ? newIndices[h.parent] : u32(-1),
            .firstChild = FindLastNonDeletedItem(scene, newIndices, h.firstChild),
            .nextSibling = FindLastNonDeletedItem(scene, newIndices, h.nextSibling),
            .lastSibling = FindLastNonDeletedItem(scene, newIndices, h.lastSibling)};
    };
    std::transform(scene.hierarchy.begin(), scene.hierarchy.end(), scene.hierarchy.begin(),
                   nodeMover);

    // 3) Finally throw away the hierarchy items
    EraseSelected(scene.hierarchy, indicesToDelete);

    // 4) As in mergeScenes() routine we also have to adjust all the "components" (i.e., meshesMap,
    // materials, names and transformations)

    // 4a) Transformations are stored in arrays, so we just erase the items as we did with the
    // scene.hierarchy
    EraseSelected(scene.localTransforms, indicesToDelete);
    EraseSelected(scene.globalTransforms, indicesToDelete);

    // 4b) All the maps should change the key values with the newIndices[] array
    ShiftMapIndices(scene.meshesMap, newIndices);
    ShiftMapIndices(scene.materialsMap, newIndices);
    ShiftMapIndices(scene.namesMap, newIndices);

    // 5) scene node names list is not modified, but in principle it can be (remove all non-used
    // items and adjust the namesMap map)

    // 6) Material names list is not modified also, but if some
    // materials fell out of use, remove them completely.
}

} // namespace Nerine
