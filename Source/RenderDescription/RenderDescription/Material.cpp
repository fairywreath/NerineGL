#include "Material.h"
#include "Utils.h"

#include <Core/Logger.h>

#include <fstream>
#include <unordered_map>

namespace Nerine
{

bool SaveMaterials(const std::string& fileName, const std::vector<MaterialDescription>& materials,
                   const std::vector<std::string>& files)
{
    std::ofstream file(fileName, std::ios::out | std::ios::binary);
    if (!file)
    {
        LOG_ERROR("SaveMaterials: failed to open file ", fileName);
        return false;
    }

    u32 size = (u32)materials.size();
    file.write((const char*)&size, sizeof(size));
    file.write((const char*)materials.data(), sizeof(MaterialDescription) * size);
    SaveStringArray(file, files);

    file.close();
    return true;
}

bool LoadMaterials(const std::string& fileName, std::vector<MaterialDescription>& materials,
                   std::vector<std::string>& files)
{
    std::ifstream file(fileName, std::ios::out | std::ios::binary);
    if (!file)
    {
        LOG_ERROR("loadMaterials: failed to open file ", fileName);
        return false;
    }

    u32 size;
    file.read((char*)&size, sizeof(size));
    materials.resize(size);
    file.read((char*)materials.data(), sizeof(MaterialDescription) * materials.size());
    LoadStringArray(file, files);

    file.close();
    return true;
}

void MergeMaterialLists(std::vector<MaterialDescription>& dstMaterials,
                        std::vector<std::string>& dstTextures,
                        const std::vector<std::vector<MaterialDescription>*>& srcMaterials,
                        const std::vector<std::vector<std::string>*>& srcTextures)
{
    // Map material description index in dstMaterials to texture list index of srcTextures.
    std::unordered_map<int, int> materialToTextureList;

    int matIndex = 0;
    for (const std::vector<MaterialDescription>* ml : srcMaterials)
    {
        for (const MaterialDescription& m : *ml)
        {
            dstMaterials.push_back(m);
            materialToTextureList[dstMaterials.size() - 1] = matIndex;
        }

        matIndex++;
    }

    // Map texture name to new texture list(dstTextures).
    std::unordered_map<std::string, int> newTextureNames;

    // Merge all texture files into 1 list.
    for (const std::vector<std::string>* tl : srcTextures)
        for (const std::string& file : *tl)
        {
            newTextureNames[file] = AddUnique(dstTextures, file);
        }

    // Replaces old material index to new index for the merged texture files list.
    auto NewTextureID = [&](int m, u64 textureID) {
        if (textureID != INVALID_TEXTURE)
        {
            auto listIndex = materialToTextureList[m];
            auto texureList = srcTextures[listIndex];

            const std::string& textureFile = (*texureList)[textureID];
            return (u64)(newTextureNames[textureFile]);
        }

        return textureID;
    };

    for (size_t i = 0; i < srcMaterials.size(); i++)
    {
        auto& m = dstMaterials[i];
        m.ambientOcclusionMap = NewTextureID(i, m.ambientOcclusionMap);
        m.emissiveMap = NewTextureID(i, m.emissiveMap);
        m.albedoMap = NewTextureID(i, m.albedoMap);
        m.metallicRoughnessMap = NewTextureID(i, m.metallicRoughnessMap);
        m.normalMap = NewTextureID(i, m.normalMap);
    }
}

} // namespace Nerine
