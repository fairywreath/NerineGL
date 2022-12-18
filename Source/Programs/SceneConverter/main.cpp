#include <algorithm>
#include <execution>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/pbrmaterial.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>
#include <stb_image_resize.h>
#include <stb_image_write.h>

#include <meshoptimizer.h>

#include <Logger.h>

#include <RenderDescription/Material.h>
#include <RenderDescription/Mesh.h>
#include <RenderDescription/Scene.h>
#include <RenderDescription/Utils.h>

namespace fs = std::filesystem;

struct SceneConfig
{
    std::string fileName;
    std::string outputMesh;
    std::string outputScene;
    std::string outputMaterials;

    float scale;
    bool calculateLODs;
    bool mergeInstances{false};
};

glm::mat4 ToMat4(const aiMatrix4x4& from)
{
    glm::mat4 to;
    to[0][0] = (float)from.a1;
    to[0][1] = (float)from.b1;
    to[0][2] = (float)from.c1;
    to[0][3] = (float)from.d1;
    to[1][0] = (float)from.a2;
    to[1][1] = (float)from.b2;
    to[1][2] = (float)from.c2;
    to[1][3] = (float)from.d2;
    to[2][0] = (float)from.a3;
    to[2][1] = (float)from.b3;
    to[2][2] = (float)from.c3;
    to[2][3] = (float)from.d3;
    to[3][0] = (float)from.a4;
    to[3][1] = (float)from.b4;
    to[3][2] = (float)from.c4;
    to[3][3] = (float)from.d4;
    return to;
}

void Traverse(const aiScene* sourceScene, Scene& scene, aiNode* N, int parent, int ofs)
{
    int newNode = AddNode(scene, parent, ofs);

    if (N->mName.C_Str())
    {
        u32 stringID = (u32)scene.names.size();
        scene.names.push_back(std::string(N->mName.C_Str()));
        scene.namesMap[newNode] = stringID;
    }

    for (size_t i = 0; i < N->mNumMeshes; i++)
    {
        int newSubNode = AddNode(scene, newNode, ofs + 1);

        u32 stringID = (u32)scene.names.size();
        scene.names.push_back(std::string(N->mName.C_Str()) + "_Mesh_" + std::to_string(i));
        scene.namesMap[newSubNode] = stringID;

        int mesh = (int)N->mMeshes[i];
        scene.meshesMap[newSubNode] = mesh;
        scene.materialsMap[newSubNode] = sourceScene->mMeshes[mesh]->mMaterialIndex;

        scene.globalTransforms[newSubNode] = glm::mat4(1.0f);
        scene.localTransforms[newSubNode] = glm::mat4(1.0f);
    }

    scene.globalTransforms[newNode] = glm::mat4(1.0f);
    scene.localTransforms[newNode] = ToMat4(N->mTransformation);

    for (unsigned int n = 0; n < N->mNumChildren; n++)
        Traverse(sourceScene, scene, N->mChildren[n], newNode, ofs + 1);
}

std::string ReplaceAll(const std::string& str, const std::string& oldSubStr,
                       const std::string& newSubStr)
{
    std::string result = str;

    for (size_t p = result.find(oldSubStr); p != std::string::npos; p = result.find(oldSubStr))
        result.replace(p, oldSubStr.length(), newSubStr);

    return result;
}

std::string LowercaseString(const std::string& s)
{
    std::string out(s.length(), ' ');
    std::transform(s.begin(), s.end(), out.begin(), tolower);
    return out;
}

std::string FindSubstitute(const std::string& origFile)
{
    auto apath = fs::absolute(fs::path(origFile));
    auto afile = LowercaseString(apath.filename().string());
    auto dir = fs::path(origFile).remove_filename();

    // Iterate each file non-recursively and compare lowercase absolute path with 'afile'
    for (auto& p : fs::directory_iterator(dir))
        if (afile == LowercaseString(p.path().filename().string()))
            return p.path().string();

    return std::string{};
}

std::string FixTextureFile(const std::string& file)
{
    return fs::exists(file) ? file : FindSubstitute(file);
}

std::string GetFileBaseName(const std::string& path)
{
    return path.substr(path.find_last_of("/\\") + 1);
}

std::string ConvertTexture(const std::string& file, const std::string& basePath,
                           std::unordered_map<std::string, u32>& opacityMapIndices,
                           const std::vector<std::string>& opacityMaps)
{
    const auto maxNewWidth = 512;
    const auto maxNewHeight = 512;

    const auto srcFile = ReplaceAll(basePath + file, "\\", "/");

    // XXX: Make this configurable from scene config.
    const auto newFile = "bistro_textures/" +
                         // GetFileBaseName(srcFile)
                         LowercaseString(ReplaceAll(ReplaceAll(srcFile, "..", "_"), "/", "_"))
                         + "_r.png";

    // Load the image.
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(FixTextureFile(srcFile).c_str(), &texWidth, &texHeight,
                                &texChannels, STBI_rgb_alpha);
    u8* src = pixels;
    texChannels = STBI_rgb_alpha;

    std::vector<u8> tmpImage(maxNewWidth * maxNewHeight * 4);

    if (!src)
    {
        LOG_ERROR("Failed to load texture: ", srcFile);
        texWidth = maxNewWidth;
        texHeight = maxNewHeight;
        texChannels = STBI_rgb_alpha;
        src = tmpImage.data();
    }
    else
    {
        LOG_INFO("Load ", srcFile, " texture with ", texWidth, " ", texHeight, " dimensions and ",
                 texChannels, " channels");
    }

    if (opacityMapIndices.count(file) > 0)
    {
        const auto opacityMapFile
            = ReplaceAll(basePath + opacityMaps[opacityMapIndices[file]], "\\", "/");
        int opacityWidth, opacityHeight;
        stbi_uc* opacityPixels = stbi_load(FixTextureFile(opacityMapFile).c_str(), &opacityWidth,
                                           &opacityHeight, nullptr, 1);

        if (!opacityPixels)
        {
            LOG_ERROR("Failed to load opacity mask: ", opacityMapFile);
        }

        assert(opacityPixels);
        assert(texWidth == opacityWidth);
        assert(texHeight == opacityHeight);

        // Store the opacity mask in the alpha component of this image.
        if (opacityPixels)
            for (int y = 0; y != opacityHeight; y++)
                for (int x = 0; x != opacityWidth; x++)
                    src[(y * opacityWidth + x) * texChannels + 3]
                        = opacityPixels[y * opacityWidth + x];

        stbi_image_free(opacityPixels);
    }

    const u32 imgSize = texWidth * texHeight * texChannels;
    std::vector<u8> mipData(imgSize);
    uint8_t* dst = mipData.data();

    const int newW = std::min(texWidth, maxNewWidth);
    const int newH = std::min(texHeight, maxNewHeight);

    stbir_resize_uint8(src, texWidth, texHeight, 0, dst, newW, newH, 0, texChannels);

    int write_res = stbi_write_png(newFile.c_str(), newW, newH, texChannels, dst, 0);

    if (pixels)
        stbi_image_free(pixels);

    return newFile;
}

void ConvertAndDownscaleAllTextures(const std::vector<MaterialDescription>& materials,
                                    const std::string& basePath, std::vector<std::string>& files,
                                    std::vector<std::string>& opacityMaps)
{
    std::unordered_map<std::string, u32> opacityMapIndices(files.size());

    for (const auto& m : materials)
        if (m.opacityMap != INVALID_TEXTURE && m.albedoMap != INVALID_TEXTURE)
            opacityMapIndices[files[m.albedoMap]] = (u32)m.opacityMap;

    auto converter = [&](const std::string& s) -> std::string {
        return ConvertTexture(s, basePath, opacityMapIndices, opacityMaps);
    };

    std::transform(std::execution::par, std::begin(files), std::end(files), std::begin(files),
                   converter);
}

static constexpr auto NUM_VERTEX_ELEMENTS = 3 + 3 + 2;

MaterialDescription ConvertAIMaterialToMaterialDescription(const aiMaterial* M,
                                                           std::vector<std::string>& files,
                                                           std::vector<std::string>& opacityMaps)
{
    MaterialDescription matDescription;

    aiColor4D Color;

    if (aiGetMaterialColor(M, AI_MATKEY_COLOR_AMBIENT, &Color) == AI_SUCCESS)
    {
        matDescription.emissiveColor = {Color.r, Color.g, Color.b, Color.a};
        if (matDescription.emissiveColor.w > 1.0f)
            matDescription.emissiveColor.w = 1.0f;
    }
    if (aiGetMaterialColor(M, AI_MATKEY_COLOR_DIFFUSE, &Color) == AI_SUCCESS)
    {
        matDescription.albedoColor = {Color.r, Color.g, Color.b, Color.a};
        if (matDescription.albedoColor.w > 1.0f)
            matDescription.albedoColor.w = 1.0f;
    }
    if (aiGetMaterialColor(M, AI_MATKEY_COLOR_EMISSIVE, &Color) == AI_SUCCESS)
    {
        matDescription.emissiveColor.x += Color.r;
        matDescription.emissiveColor.y += Color.g;
        matDescription.emissiveColor.z += Color.b;
        matDescription.emissiveColor.w += Color.a;
        if (matDescription.emissiveColor.w > 1.0f)
            matDescription.albedoColor.w = 1.0f;
    }

    const float opaquenessThreshold = 0.05f;
    float Opacity = 1.0f;

    if (aiGetMaterialFloat(M, AI_MATKEY_OPACITY, &Opacity) == AI_SUCCESS)
    {
        matDescription.transparencyFactor = glm::clamp(1.0f - Opacity, 0.0f, 1.0f);
        if (matDescription.transparencyFactor >= 1.0f - opaquenessThreshold)
            matDescription.transparencyFactor = 0.0f;
    }

    if (aiGetMaterialColor(M, AI_MATKEY_COLOR_TRANSPARENT, &Color) == AI_SUCCESS)
    {
        const float Opacity = std::max(std::max(Color.r, Color.g), Color.b);
        matDescription.transparencyFactor = glm::clamp(Opacity, 0.0f, 1.0f);
        if (matDescription.transparencyFactor >= 1.0f - opaquenessThreshold)
            matDescription.transparencyFactor = 0.0f;
        matDescription.alphaTest = 0.5f;
    }

    float tmp = 1.0f;
    if (aiGetMaterialFloat(M, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR, &tmp)
        == AI_SUCCESS)
        matDescription.metallicFactor = tmp;

    if (aiGetMaterialFloat(M, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR, &tmp)
        == AI_SUCCESS)
        matDescription.roughness = {tmp, tmp, tmp, tmp};

    aiString Path;
    aiTextureMapping Mapping;
    unsigned int UVIndex = 0;
    float Blend = 1.0f;
    aiTextureOp TextureOp = aiTextureOp_Add;
    aiTextureMapMode TextureMapMode[2] = {aiTextureMapMode_Wrap, aiTextureMapMode_Wrap};
    unsigned int TextureFlags = 0;

    if (aiGetMaterialTexture(M, aiTextureType_EMISSIVE, 0, &Path, &Mapping, &UVIndex, &Blend,
                             &TextureOp, TextureMapMode, &TextureFlags)
        == AI_SUCCESS)
    {
        matDescription.emissiveMap = AddUnique(files, Path.C_Str());
    }

    if (aiGetMaterialTexture(M, aiTextureType_DIFFUSE, 0, &Path, &Mapping, &UVIndex, &Blend,
                             &TextureOp, TextureMapMode, &TextureFlags)
        == AI_SUCCESS)
    {
        matDescription.albedoMap = AddUnique(files, Path.C_Str());
        const std::string albedoMap = std::string(Path.C_Str());
        if (albedoMap.find("grey_30") != albedoMap.npos)
            matDescription.flags |= u32(MaterialFlags::TRANSPARENT);
    }

    // first try tangent space normal map
    if (aiGetMaterialTexture(M, aiTextureType_NORMALS, 0, &Path, &Mapping, &UVIndex, &Blend,
                             &TextureOp, TextureMapMode, &TextureFlags)
        == AI_SUCCESS)
    {
        matDescription.normalMap = AddUnique(files, Path.C_Str());
    }
    // then height map
    if (matDescription.normalMap == 0xFFFFFFFF)
        if (aiGetMaterialTexture(M, aiTextureType_HEIGHT, 0, &Path, &Mapping, &UVIndex, &Blend,
                                 &TextureOp, TextureMapMode, &TextureFlags)
            == AI_SUCCESS)
            matDescription.normalMap = AddUnique(files, Path.C_Str());

    if (aiGetMaterialTexture(M, aiTextureType_OPACITY, 0, &Path, &Mapping, &UVIndex, &Blend,
                             &TextureOp, TextureMapMode, &TextureFlags)
        == AI_SUCCESS)
    {
        matDescription.opacityMap = AddUnique(opacityMaps, Path.C_Str());
        matDescription.alphaTest = 0.5f;
    }

    // patch materials
    aiString Name;
    std::string materialName;
    if (aiGetMaterialString(M, AI_MATKEY_NAME, &Name) == AI_SUCCESS)
    {
        materialName = Name.C_Str();
    }

    // apply heuristics for specific materials
    if ((materialName.find("Glass") != std::string::npos)
        || (materialName.find("Vespa_Headlight") != std::string::npos))
    {
        matDescription.alphaTest = 0.75f;
        matDescription.transparencyFactor = 0.1f;
        matDescription.flags |= u32(MaterialFlags::TRANSPARENT);
    }
    else if (materialName.find("Bottle") != std::string::npos)
    {
        matDescription.alphaTest = 0.54f;
        matDescription.transparencyFactor = 0.4f;
        matDescription.flags |= u32(MaterialFlags::TRANSPARENT);
    }
    else if (materialName.find("Metal") != std::string::npos)
    {
        matDescription.metallicFactor = 1.0f;
        matDescription.roughness = GPUVec4(0.1f, 0.1f, 0.0f, 0.0f);
    }

    return matDescription;
}

void ProcessLods(std::vector<u32>& indices, std::vector<float>& vertices,
                 std::vector<std::vector<u32>>& outLods)
{
    size_t verticesCountIn = vertices.size() / 2;
    size_t targetIndicesCount = indices.size();

    u8 LOD = 1;

    LOG_INFO("processLods: indices count at the start ", indices.size());

    outLods.push_back(indices);

    while (targetIndicesCount > 1024 && LOD < 8)
    {
        targetIndicesCount = indices.size() / 2;

        bool sloppy = false;

        size_t numOptIndices
            = meshopt_simplify(indices.data(), indices.data(), (u32)indices.size(), vertices.data(),
                               verticesCountIn, sizeof(float) * 3, targetIndicesCount, 0.02f);

        // cannot simplify further
        if (static_cast<size_t>(numOptIndices * 1.1f) > indices.size())
        {
            if (LOD > 1)
            {
                // try harder
                numOptIndices = meshopt_simplifySloppy(
                    indices.data(), indices.data(), indices.size(), vertices.data(),
                    verticesCountIn, sizeof(float) * 3, targetIndicesCount, 0.02f, nullptr);
                sloppy = true;
                if (numOptIndices == indices.size())
                    break;
            }
            else
                break;
        }

        indices.resize(numOptIndices);

        meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(),
                                    verticesCountIn);

        LOG_INFO("ProcessLods: count of indices at LOD ", LOD, ": ", numOptIndices,
                 ", sloppy:", sloppy);

        LOD++;

        outLods.push_back(indices);
    }
};

Mesh ConvertAIMesh(const aiMesh* aimesh, const SceneConfig& config, MeshData& meshData,
                   u32& indexOffset, u32& vertexOffset)
{
    const bool hasTexCoords = aimesh->HasTextureCoords(0);
    const u32 streamElementSize = static_cast<u32>(NUM_VERTEX_ELEMENTS * sizeof(float));

    // Original data for LOD calculation
    std::vector<float> srcVertices;
    std::vector<u32> srcIndices;

    std::vector<std::vector<u32>> outLods;

    auto& vertices = meshData.vertexData;

    for (auto i = 0; i != aimesh->mNumVertices; i++)
    {
        // vertices
        const aiVector3D v = aimesh->mVertices[i];

        // normals
        const aiVector3D n = aimesh->mNormals[i];

        // texcoords
        const aiVector3D t = hasTexCoords ? aimesh->mTextureCoords[0][i] : aiVector3D();

        if (config.calculateLODs)
        {
            srcVertices.push_back(v.x);
            srcVertices.push_back(v.y);
            srcVertices.push_back(v.z);
        }

        vertices.push_back(v.x * config.scale);
        vertices.push_back(v.y * config.scale);
        vertices.push_back(v.z * config.scale);

        vertices.push_back(t.x);
        vertices.push_back(1.0f - t.y);

        vertices.push_back(n.x);
        vertices.push_back(n.y);
        vertices.push_back(n.z);
    }

    Mesh result = {
        .vertexCount = aimesh->mNumVertices,
        .vertexOffset = vertexOffset,
        .indexOffset = indexOffset,
        .streamCount = 1,
        .streamOffset = {vertexOffset * streamElementSize},
        .streamElementSize = {streamElementSize},
    };

    for (auto i = 0; i != aimesh->mNumFaces; i++)
    {
        if (aimesh->mFaces[i].mNumIndices != 3)
            continue;
        for (auto j = 0; j != aimesh->mFaces[i].mNumIndices; j++)
            srcIndices.push_back(aimesh->mFaces[i].mIndices[j]);
    }

    if (!config.calculateLODs)
        outLods.push_back(srcIndices);
    else
        ProcessLods(srcIndices, srcVertices, outLods);

    u32 numIndices = 0;
    for (auto l = 0; l < outLods.size(); l++)
    {
        for (auto i = 0; i < outLods[l].size(); i++)
            meshData.indexData.push_back(outLods[l][i]);

        result.lodOffset[l] = numIndices;
        numIndices += (int)outLods[l].size();
    }

    result.lodOffset[outLods.size()] = numIndices;
    result.lodCount = (u32)outLods.size();

    indexOffset += numIndices;
    vertexOffset += aimesh->mNumVertices;

    return result;
}

void ProcessScene(const SceneConfig& config)
{
    MeshData meshData;
    u32 indexOffset = 0;
    u32 vertexOffset = 0;

    const std::size_t pathSeparator = config.fileName.find_last_of("/\\");
    const std::string basePath = (pathSeparator != std::string::npos)
                                     ? config.fileName.substr(0, pathSeparator + 1)
                                     : std::string();

    const unsigned int flags = 0 | aiProcess_JoinIdenticalVertices | aiProcess_Triangulate
                               | aiProcess_GenSmoothNormals | aiProcess_LimitBoneWeights
                               | aiProcess_SplitLargeMeshes | aiProcess_ImproveCacheLocality
                               | aiProcess_RemoveRedundantMaterials | aiProcess_FindDegenerates
                               | aiProcess_FindInvalidData | aiProcess_GenUVCoords;

    LOG_INFO("Importing model: ", config.fileName);

    Assimp::Importer import;
    const aiScene* scene = import.ReadFile(config.fileName.c_str(), flags);

    if (!scene || !scene->HasMeshes())
    {
        LOG_ERROR("Unable to load: ", fs::absolute(config.fileName));
        exit(EXIT_FAILURE);
    }

    // 1. Mesh conversion.
    meshData.meshes.reserve(scene->mNumMeshes);
    meshData.boundingBoxes.reserve(scene->mNumMeshes);

    for (unsigned int i = 0; i != scene->mNumMeshes; i++)
    {
        LOG_INFO("Converting meshes,  ", i + 1, "/", scene->mNumMeshes, "...");
        Mesh mesh = ConvertAIMesh(scene->mMeshes[i], config, meshData, indexOffset, vertexOffset);
        meshData.meshes.push_back(mesh);
    }

    RecalculateBoundingBoxes(meshData);

    SaveMeshData(config.outputMesh.c_str(), meshData);

    Scene ourScene;

    // 2. Material conversion.
    std::vector<MaterialDescription> materials;
    std::vector<std::string>& materialNames = ourScene.materialNames;

    std::vector<std::string> files;
    std::vector<std::string> opacityMaps;

    for (unsigned int m = 0; m < scene->mNumMaterials; m++)
    {
        aiMaterial* mm = scene->mMaterials[m];

        LOG_INFO("Material [", mm->GetName().C_Str(), "] ", m);
        materialNames.push_back(std::string(mm->GetName().C_Str()));

        MaterialDescription D = ConvertAIMaterialToMaterialDescription(mm, files, opacityMaps);
        materials.push_back(D);
    }

    // 3. Texture processing, rescaling and packing.
    ConvertAndDownscaleAllTextures(materials, basePath, files, opacityMaps);

    SaveMaterials(config.outputMaterials, materials, files);

    // 4. Scene hierarchy conversion.
    Traverse(scene, ourScene, scene->mRootNode, -1, 0);

    SaveScene(config.outputScene, ourScene);
}

int main()
{
    LOG_SET_OUTPUT(&std::cout);
    LOG_INFO("Running scene conversion...");

    std::vector<SceneConfig> sceneConfigs{
        {
            .fileName = "../Resources/Bistro/Exterior/exterior.obj",
            .outputMesh = "../Resources/Bistro/exterior.meshes",
            .outputScene = "../Resources/Bistro/exterior.scene",
            .outputMaterials = "../Resources/Bistro/exterior.materials",
            .scale = 0.01,
            .calculateLODs = false,
            .mergeInstances = false,
        },
        /* {
             .fileName = "../../../../../Resources/bistro/Interior/interior.obj",
             .outputMesh = "../../../../../Resources/bistro/interior.meshes",
             .outputScene = "../../../../../Resources/bistro/interior.scene",
             .outputMaterials = "../../../../../Resources/bistro/interior.materials",
             .scale = 0.01,
             .calculateLODs = false,
             .mergeInstances = false,
         },*/
    };

    for (const auto& config : sceneConfigs)
    {
        ProcessScene(config);
    }

    LOG_INFO("Conversion done!");

    return 0;
}
