#pragma once

#include <RenderDescription/Material.h>
#include <RenderDescription/Mesh.h>
#include <RenderDescription/Scene.h>

#include "GLResources.h"

#include <functional>

namespace Nerine
{

constexpr GLuint BUFFER_INDEX_PERFRAME_UNIFORMS = 0;
constexpr GLuint BUFFER_INDEX_MODEL_MATRICES = 1;
constexpr GLuint BUFFER_INDEX_MATERIALS = 2;

/*
 * Shader structure types.
 */
struct GPUSceneData
{
    mat4 view;
    mat4 proj;

    // Global directional shadow light, mainly used for shadows.
    mat4 light;

    vec4 cameraPos;
    vec4 frustumPlanes[6];
    vec4 frustumCorners[8];
    u32 numShapesToCull;
};

struct GPUSSAOParams
{
    float scale{1.5f};
    float bias{0.15f};
    float zNear{0.1f};
    float zFar{1000.0f};
    float radius{0.05f};
    float attScale{1.01f};
    float distScale{0.6f};
};

struct GPUHDRParams
{
    float exposure{0.9f};
    float maxWhite{1.17f};
    float bloomStrength{1.1f};
    float adaptationSpeed{0.1f};
};

static_assert(sizeof(GPUSSAOParams) <= sizeof(GPUSceneData));
static_assert(sizeof(GPUHDRParams) <= sizeof(GPUSceneData));

// Transparency linked-list "node".
struct GPUTransparentFragment
{
    // vec4 in the shader.
    float r;
    float g;
    float b;
    float a;

    float depth;
    u32 next;
};

/*
 * Draw scene data.
 */
class GLSceneData
{
public:
    GLSceneData() = default;
    GLSceneData(const std::string& meshFile, const std::string& sceneFile,
                const std::string& materialFile);

    void Load(const std::string& meshFile, const std::string& sceneFile,
              const std::string& materialFile);
    void LoadSceneFile(const std::string& sceneFile);

    std::vector<TextureHandle> materialTextures;

    MeshFileHeader meshHeader;
    MeshData meshData;

    Scene scene;

    std::vector<MaterialDescription> materials;
    std::vector<DrawData> shapes;
};

struct DrawElementsIndirectCommand
{
    GLuint count;
    GLuint instanceCount;
    GLuint firstIndex;
    GLuint baseVertex;
    GLuint baseInstance;
};

/*
 * Indirect draw buffer.
 */
class GLIndirectBuffer : public RefCountResource<IResource>
{
public:
    using IndirectBufferHandle = RefCountPtr<GLIndirectBuffer>;

    explicit GLIndirectBuffer(size_t maxDrawCommands);

    void UploadIndirectBuffer();

    /*
     * Select and put specific draw commands to the outputBuffer based on the given predicate
     * function.
     */
    void SelectDrawCommands(
        IndirectBufferHandle outputBuffer,
        const std::function<bool(const DrawElementsIndirectCommand&)>& predicate);

    std::vector<DrawElementsIndirectCommand> m_DrawCommands;

    GLuint m_Handle;

    BufferHandle m_BufferIndirect;
};

using IndirectBufferHandle = RefCountPtr<GLIndirectBuffer>;

IndirectBufferHandle CreateIndirectBuffer(size_t maxDrawCommands);

class GLMesh final
{
public:
    GLMesh() = default;
    explicit GLMesh(GLSceneData& sceneData);
    ~GLMesh();

    NON_COPYABLE(GLMesh);
    NON_MOVEABLE(GLMesh);

    void LoadSceneData(GLSceneData& sceneData);

    void Draw(u32 numDrawCommands, IndirectBufferHandle indirectBuffer = nullptr) const;

    GLuint m_Vao{0};
    u32 m_NumIndices;

    BufferHandle m_BufferIndices;
    BufferHandle m_BufferVertices;
    BufferHandle m_BufferMaterials;
    BufferHandle m_BufferModelMatrices;

    IndirectBufferHandle m_BufferIndirect;

    // XXX: Hold strong reference?
    GLSceneData* m_SceneData{nullptr};
};

class SkyboxRenderer
{
public:
    SkyboxRenderer(const std::string& envMapFile, const std::string& irradianceFile);
    ~SkyboxRenderer();

    void Draw();

private:
    TextureHandle m_EnvMap;
    TextureHandle m_EnvMapIrradiance;
    TextureHandle m_BrdfLUT{CreateTexture(GL_TEXTURE_2D, "Resources/brdfLUT.ktx")};

    ShaderHandle m_CubeVS{CreateShader("Shaders/Skybox/Cube.vs.glsl")};
    ShaderHandle m_CubeFS{CreateShader("Shaders/Skybox/Cube.fs.glsl")};

    ProgramHandle m_CubeProgram{CreateProgram(m_CubeVS, m_CubeFS)};

    GLuint m_VAO{0};
};

} // namespace Nerine
