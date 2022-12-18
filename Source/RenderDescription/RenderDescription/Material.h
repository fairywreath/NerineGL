#pragma once

#include <Core/Types.h>

#include <string>
#include <vector>

namespace Nerine
{

constexpr u64 INVALID_TEXTURE = 0xFFFFFFFF;

// Extra flags.
enum class MaterialFlags : u8
{
    NONE = 0,
    TRANSPARENT = 1,
};

ENUM_CLASS_FLAG_OPERATORS(MaterialFlags);

struct PACKED_STRUCT MaterialDescription
{
    GPUVec4 emissiveColor{0.0f};
    GPUVec4 albedoColor{1.0f};
    // Only first 2 values are used.
    GPUVec4 roughness{1.0f, 1.0f, 0.0f, 0.0f};

    float transparencyFactor{1.0f};
    float alphaTest{1.0f};
    float metallicFactor{0.0f};

    // XXX: Handle flags nicely.
    u32 flags{u32(MaterialFlags::NONE)};

    // Index to texture names/array.
    u64 ambientOcclusionMap{INVALID_TEXTURE};
    u64 emissiveMap{INVALID_TEXTURE};
    u64 albedoMap{INVALID_TEXTURE};
    u64 metallicRoughnessMap{INVALID_TEXTURE};
    u64 normalMap{INVALID_TEXTURE};
    u64 opacityMap{INVALID_TEXTURE};
}; // namespace Nerine

static_assert(sizeof(MaterialDescription) % 16 == 0,
              "MaterialDescription must be padded to 16 bytes!");

bool SaveMaterials(const std::string& fileName, const std::vector<MaterialDescription>& materials,
                   const std::vector<std::string>& files);
bool LoadMaterials(const std::string& fileName, std::vector<MaterialDescription>& materials,
                   std::vector<std::string>& files);

// XXX: Need to handle case where merged texture array size exceeds device shader limits.
void MergeMaterialLists(std::vector<MaterialDescription>& dstMaterials,
                        std::vector<std::string>& dstTextures,
                        const std::vector<std::vector<MaterialDescription>*>& srcMaterials,
                        const std::vector<std::vector<std::string>*>& srcTextures);

} // namespace Nerine
