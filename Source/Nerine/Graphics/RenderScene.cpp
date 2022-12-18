#include "RenderScene.h"

#include <Core/Logger.h>

#include <unordered_map>

namespace Nerine
{

namespace
{

GLuint64 GetTextureHandleBindless(u64 index, const std::vector<TextureHandle>& textures)
{
    if (index == INVALID_TEXTURE)
        return 0;

    return textures[index]->m_HandleBindless;
}

} // namespace

GLSceneData::GLSceneData(const std::string& meshFile, const std::string& sceneFile,
                         const std::string& materialFile)
{
    Load(meshFile, sceneFile, materialFile);
}

void GLSceneData::Load(const std::string& meshFile, const std::string& sceneFile,
                       const std::string& materialFile)
{
    meshHeader = LoadMeshData(meshFile, meshData);
    LoadSceneFile(sceneFile);

    std::vector<std::string> textureFiles;
    LoadMaterials(materialFile, materials, textureFiles);

    std::unordered_map<std::string, u32> fnMap;

    for (const auto& file : textureFiles)
    {
        materialTextures.push_back(TextureHandle::Create(new GLTexture(GL_TEXTURE_2D, file)));
        if (!fnMap.contains(file))
        {
            fnMap[file] = 0;
        }
    }

    LOG_INFO("Unique files count: ", fnMap.size());

    int idx = 0;
    for (auto& material : materials)
    {
        auto matName = scene.materialNames[idx];

        material.ambientOcclusionMap
            = GetTextureHandleBindless(material.ambientOcclusionMap, materialTextures);
        material.emissiveMap = GetTextureHandleBindless(material.emissiveMap, materialTextures);
        material.albedoMap = GetTextureHandleBindless(material.albedoMap, materialTextures);
        material.metallicRoughnessMap
            = GetTextureHandleBindless(material.metallicRoughnessMap, materialTextures);
        material.normalMap = GetTextureHandleBindless(material.normalMap, materialTextures);

        idx++;
    }
}

void GLSceneData::LoadSceneFile(const std::string& sceneFile)
{
    auto res = LoadScene(sceneFile, scene);
    if (!res)
    {
        LOG_ERROR("Failed to load scene file: ", sceneFile);
        return;
    }

    for (const auto& c : scene.meshesMap)
    {
        auto material = scene.materialsMap.find(c.first);
        if (material != scene.materialsMap.end())
        {
            shapes.push_back(DrawData{.meshIndex = c.second,
                                      .materialIndex = material->second,
                                      .LOD = 0,
                                      .indexOffset = meshData.meshes[c.second].indexOffset,
                                      .vertexOffset = meshData.meshes[c.second].vertexOffset,
                                      .transformIndex = c.first});
        }
    }

    MarkAsChanged(scene, 0);
    RecalculateGlobalTransforms(scene);
}

SkyboxRenderer::SkyboxRenderer(const std::string& envMapFile, const std::string& irradianceFile)
    : m_EnvMap(CreateTexture(GL_TEXTURE_CUBE_MAP, envMapFile)),
      m_EnvMapIrradiance(CreateTexture(GL_TEXTURE_CUBE_MAP, irradianceFile))
{
    glCreateVertexArrays(1, &m_VAO);
    const GLuint textures[]
        = {m_EnvMap->m_Handle, m_EnvMapIrradiance->m_Handle, m_BrdfLUT->m_Handle};
    glBindTextures(5, 3, textures);
}

SkyboxRenderer::~SkyboxRenderer()
{
    // YYY REMOVE THIS: Properly handle moves.
    // glDeleteVertexArrays(1, &m_VAO);
}

void SkyboxRenderer::Draw()
{
    m_CubeProgram->Use();
    glBindTextureUnit(1, m_EnvMap->m_Handle);
    glDepthMask(false);
    glBindVertexArray(m_VAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glDepthMask(true);

    // YYY: Bind here for usage in the next pipeline. Shouldnt be here.
    const GLuint textures[]
        = {m_EnvMap->m_Handle, m_EnvMapIrradiance->m_Handle, m_BrdfLUT->m_Handle};
    glBindTextures(5, 3, textures);
}

GLIndirectBuffer::GLIndirectBuffer(size_t maxDrawCommands)
    : m_BufferIndirect(CreateBuffer(sizeof(DrawElementsIndirectCommand) * maxDrawCommands, nullptr,
                                    GL_DYNAMIC_STORAGE_BIT)),
      m_DrawCommands(maxDrawCommands)
{
    m_Handle = m_BufferIndirect->m_Handle;
}

void GLIndirectBuffer::UploadIndirectBuffer()
{
    glNamedBufferSubData(m_BufferIndirect->m_Handle, 0,
                         sizeof(DrawElementsIndirectCommand) * m_DrawCommands.size(),
                         m_DrawCommands.data());
}

void GLIndirectBuffer::SelectDrawCommands(
    IndirectBufferHandle outputBuffer,
    const std::function<bool(const DrawElementsIndirectCommand&)>& predicate)
{
    outputBuffer->m_DrawCommands.clear();
    for (const auto& drawCommand : m_DrawCommands)
    {
        if (predicate(drawCommand))
            outputBuffer->m_DrawCommands.push_back(drawCommand);
    }
    outputBuffer->UploadIndirectBuffer();
}

IndirectBufferHandle CreateIndirectBuffer(size_t maxDrawCommands)
{
    return IndirectBufferHandle::Create(new GLIndirectBuffer(maxDrawCommands));
}

GLMesh::GLMesh(GLSceneData& sceneData)
    : m_NumIndices(sceneData.meshHeader.indexDataSize / sizeof(u32)),
      m_BufferIndices(
          CreateBuffer(sceneData.meshHeader.indexDataSize, sceneData.meshData.indexData.data(), 0)),
      m_BufferVertices(CreateBuffer(sceneData.meshHeader.vertexDataSize,
                                    sceneData.meshData.vertexData.data(), 0)),
      m_BufferMaterials(CreateBuffer(sizeof(MaterialDescription) * sceneData.materials.size(),
                                     sceneData.materials.data(), 0)),
      m_BufferModelMatrices(CreateBuffer(sizeof(glm::mat4) * sceneData.shapes.size(), nullptr,
                                         GL_DYNAMIC_STORAGE_BIT)),
      m_BufferIndirect(CreateIndirectBuffer(sceneData.shapes.size()))
{
    LoadSceneData(sceneData);
}

GLMesh::~GLMesh()
{
    if (m_Vao != 0)
    {
        glDeleteVertexArrays(1, &m_Vao);
    }
}

void GLMesh::LoadSceneData(GLSceneData& sceneData)
{
    // XXX: Need this? Strong reference needed?
    m_SceneData = &sceneData;

    glCreateVertexArrays(1, &m_Vao);
    glVertexArrayElementBuffer(m_Vao, m_BufferIndices->m_Handle);
    glVertexArrayVertexBuffer(m_Vao, 0, m_BufferVertices->m_Handle, 0,
                              sizeof(vec3) + sizeof(vec3) + sizeof(vec2));

    // Position.
    glEnableVertexArrayAttrib(m_Vao, 0);
    glVertexArrayAttribFormat(m_Vao, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(m_Vao, 0, 0);

    // UV.
    glEnableVertexArrayAttrib(m_Vao, 1);
    glVertexArrayAttribFormat(m_Vao, 1, 2, GL_FLOAT, GL_FALSE, sizeof(vec3));
    glVertexArrayAttribBinding(m_Vao, 1, 0);

    // Normal.
    glEnableVertexArrayAttrib(m_Vao, 2);
    glVertexArrayAttribFormat(m_Vao, 2, 3, GL_FLOAT, GL_TRUE, sizeof(vec3) + sizeof(vec2));
    glVertexArrayAttribBinding(m_Vao, 2, 0);

    // Matrices to hold transform values,
    std::vector<mat4> matrices(sceneData.shapes.size());

    // Upload indirect draw commands.
    for (size_t i = 0; i != sceneData.shapes.size(); i++)
    {
        const u32 meshIdx = sceneData.shapes[i].meshIndex;
        const u32 lod = sceneData.shapes[i].LOD;
        m_BufferIndirect->m_DrawCommands[i] = {
            .count = sceneData.meshData.meshes[meshIdx].GetLODIndicesCount(lod),
            .instanceCount = 1,
            .firstIndex = sceneData.shapes[i].indexOffset,
            .baseVertex = sceneData.shapes[i].vertexOffset,
            .baseInstance = sceneData.shapes[i].materialIndex + (u32(i) << 16),
        };

        matrices[i] = sceneData.scene.globalTransforms[sceneData.shapes[i].transformIndex];
    }
    m_BufferIndirect->UploadIndirectBuffer();

    // Upload transforms.
    glNamedBufferSubData(m_BufferModelMatrices->m_Handle, 0, matrices.size() * sizeof(mat4),
                         matrices.data());
}

void GLMesh::Draw(u32 numDrawCommands, IndirectBufferHandle indirectBuffer) const
{
    glBindVertexArray(m_Vao);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_INDEX_MATERIALS, m_BufferMaterials->m_Handle);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_INDEX_MODEL_MATRICES,
                     m_BufferModelMatrices->m_Handle);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, (indirectBuffer != nullptr) ? indirectBuffer->m_Handle
                                                                      : m_BufferIndirect->m_Handle);

    glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr, (GLsizei)numDrawCommands,
                                0);
}

} // namespace Nerine