#include "Mesh.h"

#include <Core/Logger.h>

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace Nerine
{

static constexpr auto MESH_HEADER_MAGIC_NUMBER = 0x12345678;

std::vector<DrawData> CreateMeshDrawData(const MeshData& meshData)
{
    std::vector<DrawData> drawData;
    drawData.reserve(meshData.meshes.size());

    u32 vertexOffset = 0;
    for (auto i = 0; i < meshData.meshes.size(); i++)
    {
        // Does NOT set materialIndex, LOD and transformIndex.
        drawData.push_back(DrawData{
            .meshIndex = (u32)i,
            .materialIndex = 0,
            .LOD = 0,
            .indexOffset = meshData.meshes[i].indexOffset,
            .vertexOffset = vertexOffset,
            .transformIndex = 0,
        });

        vertexOffset += meshData.meshes[i].vertexCount;
    }

    return drawData;
}

void RecalculateBoundingBoxes(MeshData& meshData)
{
    meshData.boundingBoxes.clear();

    for (const auto& mesh : meshData.meshes)
    {
        const auto numIndices = mesh.GetLODIndicesCount(0);

        vec3 vmin(std::numeric_limits<float>::max());
        vec3 vmax(std::numeric_limits<float>::lowest());

        for (auto i = 0; i != numIndices; i++)
        {
            auto vtxOffset = meshData.indexData[mesh.indexOffset + i] + mesh.vertexOffset;
            const float* vf = &meshData.vertexData[vtxOffset * MAX_STREAMS];
            vmin = glm::min(vmin, vec3(vf[0], vf[1], vf[2]));
            vmax = glm::max(vmax, vec3(vf[0], vf[1], vf[2]));
        }

        meshData.boundingBoxes.emplace_back(vmin, vmax);
    }
}

bool SaveMeshData(const std::string& fileName, const MeshData& meshData)
{
    std::ofstream outFile(fileName, std::ios::out | std::ios::binary);
    if (!outFile)
    {
        LOG_ERROR("SaveMeshData: failed to open file ", fileName);
        return false;
    }

    const MeshFileHeader header = {
        .magicNumber = MESH_HEADER_MAGIC_NUMBER,
        .meshCount = (u32)meshData.meshes.size(),
        .dataBlockStartOffset
        = (u32)(sizeof(MeshFileHeader) + meshData.meshes.size() * sizeof(Mesh)),
        .indexDataSize = (u32)(meshData.indexData.size() * sizeof(u32)),
        .vertexDataSize = (u32)(meshData.vertexData.size() * sizeof(float)),
    };

    LOG_INFO("save MeshData indexDataSize: ", header.indexDataSize);
    LOG_INFO("save MeshData vertexDataSize: ", header.vertexDataSize);

    outFile.write((char*)&header, sizeof(header));
    outFile.write((char*)meshData.meshes.data(), sizeof(Mesh) * header.meshCount);
    outFile.write((char*)meshData.boundingBoxes.data(), sizeof(BoundingBox) * header.meshCount);
    outFile.write((char*)meshData.indexData.data(), header.indexDataSize);
    outFile.write((char*)meshData.vertexData.data(), header.vertexDataSize);

    outFile.close();

    return true;
}

bool SaveDrawData(const std::string& fileName, const std::vector<DrawData>& drawData)
{
    std::ofstream outFile(fileName, std::ios::out | std::ios::binary);
    if (!outFile)
    {
        LOG_ERROR("saveDrawData: failed to open file ", fileName);
        return false;
    }

    outFile.write((char*)drawData.data(), sizeof(DrawData) * drawData.size());
    outFile.close();

    return true;
}

MeshFileHeader LoadMeshData(const std::string& fileName, MeshData& meshData)
{
    MeshFileHeader header;
    header.meshCount = 0;

    std::ifstream inFile(fileName, std::ios::out | std::ios::binary);
    if (!inFile)
    {
        LOG_ERROR("loadMeshData: failed to open ", fs::absolute(fileName));
        return header;
    }

    inFile.read((char*)&header, sizeof(header));
    if (!inFile.good())
    {
        LOG_ERROR("loadMeshData: failed to read file header ", fileName);
        inFile.close();
        header.meshCount = 0;

        return header;
    }

    if (header.magicNumber != MESH_HEADER_MAGIC_NUMBER)
    {
        LOG_ERROR("loadMeshData: ", fileName, " is not a mesh type file");
        inFile.close();
        header.meshCount = 0;

        return header;
    }

    meshData.meshes.resize(header.meshCount);
    inFile.read((char*)meshData.meshes.data(), sizeof(Mesh) * header.meshCount);

    meshData.boundingBoxes.resize(header.meshCount);
    inFile.read((char*)meshData.boundingBoxes.data(), sizeof(BoundingBox) * header.meshCount);

    meshData.indexData.resize(header.indexDataSize / sizeof(u32));
    inFile.read((char*)meshData.indexData.data(), header.indexDataSize);

    meshData.vertexData.resize(header.vertexDataSize / sizeof(float));
    inFile.read((char*)meshData.vertexData.data(), header.vertexDataSize);

    if (!inFile.good())
    {
        LOG_ERROR("loadMeshData: failed to read mesh data ", fileName);
        inFile.close();
        header.meshCount = 0;

        return header;
    }

    inFile.close();

    return header;
}

std::vector<DrawData> LoadDrawData(const std::string& fileName)
{
    std::vector<DrawData> drawData;

    std::ifstream inFile(fileName, std::ios::out | std::ios::binary);
    if (!inFile)
    {
        LOG_ERROR("loadDrawData: failed to open ", fs::absolute(fileName));
        return drawData;
    }

    auto fileSize = fs::file_size(fileName);
    if (fileSize == 0)
    {
        LOG_ERROR("loadDrawData: file ", fileName, " is empty");
        inFile.close();
        return drawData;
    }

    auto drawDataCount = (u32)(fileSize / sizeof(DrawData));
    drawData.resize(drawDataCount);

    inFile.read((char*)drawData.data(), sizeof(DrawData) * drawDataCount); // or read fileSize
    if (!inFile.good())
    {
        LOG_ERROR("loadDrawData: failed to read draw data ", fileName);
        inFile.close();
        drawData.clear();

        return drawData;
    }

    inFile.close();

    return drawData;
}

} // namespace Nerine
