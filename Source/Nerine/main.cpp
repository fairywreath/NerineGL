#include <fstream>
#include <iostream>

#include <Core/Logger.h>

#include <glad/glad.h>

#include <rapidjson/document.h>

#include "Application/Window.h"

#include "Graphics/Camera.h"
#include "Graphics/GLDevice.h"
#include "Graphics/GLImGui.h"
#include "Graphics/RenderScene.h"
#include "Graphics/RenderUtils.h"

using namespace Nerine;

/*
 * Application state.
 */
struct MouseState
{
    glm::vec2 pos{glm::vec2(0.0f)};
    bool pressedLeft{false};
} mouseState;

struct RenderSettings
{
    ivec2 resolution;

    std::string meshFile{""};
    std::string sceneFile{""};
    std::string materialFile{""};

    // XXX: Environment map, light settings etc.
};

RenderSettings ParseRenderSettings(const std::string& str)
{
    rapidjson::Document d;
    d.Parse(str.c_str());

    assert(d["meshFile"].IsString());
    assert(d["sceneFile"].IsString());
    assert(d["materialFile"].IsString());
    assert(d["resolution"]["x"].IsInt());
    assert(d["resolution"]["y"].IsInt());

    RenderSettings settings;

    settings.meshFile = d["meshFile"].GetString();
    settings.sceneFile = d["sceneFile"].GetString();
    settings.materialFile = d["materialFile"].GetString();

    settings.resolution.x = d["resolution"]["x"].GetInt();
    settings.resolution.y = d["resolution"]["y"].GetInt();

    return settings;
}

RenderSettings ReadRenderSettingsFile(const std::string& fileName)
{
    std::ifstream file(fileName);
    if (!file)
    {
        LOG_ERROR("Failed to open render settings file: ", fileName);
        // XXX: Return something nicely here.
        return {};
    }

    std::string str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    return ParseRenderSettings(str);
}

/*
 * Camera.
 */
FirstPersonCameraController mainCameraController(vec3(-10.0f, 3.0f, 3.0f), vec3(0.0f, 0.0f, -1.0f),
                                                 vec3(0.0f, 1.0f, 0.0f));
Camera mainCamera(mainCameraController);

/*
 * Rendering states.
 */
struct ApplicationRenderState
{
    bool drawOpaque{true};
    bool drawTransparent{true};

    bool enableShadows{true};
    bool showLightFrustum{false};
    // Directional light for projective shadow map.
    float lightTheta{0.0f};
    float lightPhi{0.0f};

    mat4 cullingView{mainCamera.GetViewMatrix()};
    bool enableGPUCulling{true};
    bool freezeCullingView{false};

    bool enableSSAO{true};
    bool enableSSAOBlur{true};

    bool enableHDR{true};

    bool enableTAA{true};
    bool enableFXAA{false};
    bool enableMSAA_4X{false};

    AntiAliasingType aaType{AntiAliasingType::FXAA};
} renderState;

// Halton(2, 3).
float Halton(u32 i, u32 b)
{
    float f = 1.0f;
    float r = 0.0f;
    while (i > 0)
    {
        f /= static_cast<float>(b);
        r = r + f * static_cast<float>(i % b);
        i = static_cast<uint32_t>(floorf(static_cast<float>(i) / static_cast<float>(b)));
    }
    return r;
}

GPUSSAOParams ssaoParams;
GPUHDRParams hdrParams;
GPUTAAParams taaParams;
GPUFXAAParams fxaaParams;

bool nextFrame = true;
bool stepMode = false;

void GLAPIENTRY MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                                GLsizei length, const GLchar* message, const void* userParam)
{
    fprintf(stderr, "[GL CALLBACK] %s type = 0x%x, severity = 0x%x, message = %s\n",
            (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""), type, severity, message);
}

int main(int argc, char* argv[])
{
    LOG_SET_OUTPUT(&std::cout);
    LOG_DEBUG("Starting application...");

    std::string renderSettingsFileName = "Resources/default.json";
    if (argc > 1)
    {
        renderSettingsFileName = argv[1];
    }

    auto renderSettings = ReadRenderSettingsFile(renderSettingsFileName);
    if (renderSettings.meshFile.empty() || renderSettings.sceneFile.empty()
        || renderSettings.materialFile.empty())
    {
        LOG_FATAL("Failed to obtain render settings!");
        return -1;
    }

    Window window(renderSettings.resolution.x, renderSettings.resolution.y, "GL Renderer");

    // Load glad.
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        LOG_FATAL("Failed to initialize glad!");
        return -1;
    }
    glViewport(0, 0, window.GetWidth(), window.GetHeight());
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(MessageCallback, 0);

    window.AddResizeCallback(
        [](int windowWidth, int windowHeight) { glViewport(0, 0, windowWidth, windowHeight); });

    auto windowPtr = (GLFWwindow*)window.GetNativeWindow();
    glfwSetCursorPosCallback(windowPtr, [](auto* window, double x, double y) {
        int windowWidth, windowHeight;
        glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
        mouseState.pos.x = static_cast<float>(x / windowWidth);
        mouseState.pos.y = static_cast<float>(y / windowHeight);
        ImGui::GetIO().MousePos = ImVec2((float)x, (float)y);
    });
    glfwSetMouseButtonCallback(windowPtr, [](auto* window, int button, int action, int mods) {
        auto& io = ImGui::GetIO();
        const int idx
            = button == GLFW_MOUSE_BUTTON_LEFT ? 0 : button == GLFW_MOUSE_BUTTON_RIGHT ? 2 : 1;
        io.MouseDown[idx] = action == GLFW_PRESS;

        if (!io.WantCaptureMouse)
            if (button == GLFW_MOUSE_BUTTON_LEFT)
                mouseState.pressedLeft = action == GLFW_PRESS;
    });
    glfwSetKeyCallback(windowPtr,
                       [](GLFWwindow* window, int key, int scancode, int action, int mods) {
                           const bool pressed = action != GLFW_RELEASE;
                           if (key == GLFW_KEY_ESCAPE && pressed)
                               glfwSetWindowShouldClose(window, GLFW_TRUE);
                           if (key == GLFW_KEY_W)
                               mainCameraController.Movement.forward = pressed;
                           if (key == GLFW_KEY_S)
                               mainCameraController.Movement.backward = pressed;
                           if (key == GLFW_KEY_A)
                               mainCameraController.Movement.left = pressed;
                           if (key == GLFW_KEY_D)
                               mainCameraController.Movement.right = pressed;
                           if (key == GLFW_KEY_1)
                               mainCameraController.Movement.up = pressed;
                           if (key == GLFW_KEY_2)
                               mainCameraController.Movement.down = pressed;
                           if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT)
                               mainCameraController.Movement.fast = pressed;
                           if (key == GLFW_KEY_SPACE)
                               mainCameraController.SetUpVector(vec3(0.0f, 1.0f, 0.0f));
                           if (key == GLFW_KEY_P && pressed)
                               renderState.freezeCullingView = !renderState.freezeCullingView;

                           if (key == GLFW_KEY_ENTER && pressed)
                           {
                               nextFrame = true;
                           }
                           if (key == GLFW_KEY_N && pressed)
                           {
                               stepMode = !stepMode;
                           }
                       });
    mainCameraController.MaxSpeed = 1.0f;

    GLDevice gpuDevice;

    /*
     * Shaders.
     */
    // auto vsSceneMesh = CreateShader("Shaders/Scene/Mesh.vs.glsl");
    auto vsSceneMesh = CreateShader("Shaders/Scene/MeshJitter.vs.glsl");
    auto fsSceneMesh = CreateShader("Shaders/Scene/MeshIBL.fs.glsl");
    auto programSceneMesh = CreateProgram(vsSceneMesh, fsSceneMesh);
    auto vsSceneMeshNoJitter = CreateShader("Shaders/Scene/Mesh.vs.glsl");
    auto programSceneMeshNoJitter = CreateProgram(vsSceneMeshNoJitter, fsSceneMesh);

    // Generic fullscreen quad shader for post-processing.
    auto vsFullScreenQuad = CreateShader("Shaders/FullScreen/FullScreenQuad.vs.glsl");

    // OIT.
    auto fsOITMesh = CreateShader("Shaders/Scene/MeshOIT.fs.glsl");
    auto fsOIT = CreateShader("Shaders/PostProcess/OIT.fs.glsl");

    // No jitter for transparent meshes.
    auto programOITMesh = CreateProgram(vsSceneMeshNoJitter, fsOITMesh);

    auto programOIT = CreateProgram(vsFullScreenQuad, fsOIT);

    // GPU culling.
    auto csCull = CreateShader("Shaders/Culling/FrustumCull.cs.glsl");
    auto programCull = CreateProgram(csCull);

    // SSAO.
    auto fsSSAO = CreateShader("Shaders/PostProcess/SSAO.fs.glsl");
    auto fsSSAOCombine = CreateShader("Shaders/PostProcess/SSAOCombine.fs.glsl");
    auto programSSAO = CreateProgram(vsFullScreenQuad, fsSSAO);
    auto programSSAOCombine = CreateProgram(vsFullScreenQuad, fsSSAOCombine);

    // Blur.
    auto fsBlurX = CreateShader("Shaders/PostProcess/BlurX.fs.glsl");
    auto fsBlurY = CreateShader("Shaders/PostProcess/BlurY.fs.glsl");
    auto programBlurX = CreateProgram(vsFullScreenQuad, fsBlurX);
    auto programBlurY = CreateProgram(vsFullScreenQuad, fsBlurY);

    // HDR.
    auto fsHDRCombine = CreateShader("Shaders/PostProcess/HDR.fs.glsl");
    auto programHDRCombine = CreateProgram(vsFullScreenQuad, fsHDRCombine);

    auto fsLuminance = CreateShader("Shaders/PostProcess/Luminance.fs.glsl");
    auto programLuminance = CreateProgram(vsFullScreenQuad, fsLuminance);

    auto fsBrightPass = CreateShader("Shaders/PostProcess/BrightPass.fs.glsl");
    auto programBrightPass = CreateProgram(vsFullScreenQuad, fsBrightPass);

    auto fsAdaptation = CreateShader("Shaders/PostProcess/HDRAdaptation.cs.glsl");
    auto programAdaptation = CreateProgram(fsAdaptation);

    // Shadows.
    auto vsShadow = CreateShader("Shaders/Scene/Shadow.vs.glsl");
    auto fsShadow = CreateShader("Shaders/Scene/Shadow.fs.glsl");
    auto programShadowMap = CreateProgram(vsShadow, fsShadow);

    // XXX: Make this constexpr/
    const GLuint MaxNumObjects = 128 * 1024;
    const GLsizeiptr BufferSize_SceneData = sizeof(GPUSceneData);
    const GLsizeiptr BufferSize_BoundingBoxes = sizeof(BoundingBox) * MaxNumObjects;
    const GLuint BufferIndex_BoundingBoxes = BUFFER_INDEX_PERFRAME_UNIFORMS + 1;
    const GLuint BufferIndex_DrawCommands = BUFFER_INDEX_PERFRAME_UNIFORMS + 2;
    const GLuint BufferIndex_NumVisibleMeshes = BUFFER_INDEX_PERFRAME_UNIFORMS + 3;

    // Previous frame data.
    // XXX: Combine this in the perframe SceneData with proper CPU std140 packing.
    const GLuint BufferIndex_PrevFrameData = 10;
    const GLsizeiptr BufferSize_PrevFrameData = sizeof(GPUPrevFrameData);
    auto bufferPrevFrameData
        = CreateBuffer(BufferSize_PrevFrameData, nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferRange(GL_UNIFORM_BUFFER, BufferIndex_PrevFrameData, bufferPrevFrameData->m_Handle,
                      0, BufferSize_PrevFrameData);

    // Per frame scene params.
    auto bufferSceneData = CreateBuffer(BufferSize_SceneData, nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferRange(GL_UNIFORM_BUFFER, BUFFER_INDEX_PERFRAME_UNIFORMS, bufferSceneData->m_Handle,
                      0, BufferSize_SceneData);

    auto bufferBoundingBoxes
        = CreateBuffer(BufferSize_BoundingBoxes, nullptr, GL_DYNAMIC_STORAGE_BIT);
    auto bufferNumVisibleMeshes = CreateBuffer(sizeof(u32), nullptr,
                                               GL_MAP_READ_BIT | GL_MAP_WRITE_BIT
                                                   | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

    volatile u32* mappedNumVisibleMeshesPtr
        = (u32*)glMapNamedBuffer(bufferNumVisibleMeshes->m_Handle, GL_READ_WRITE);
    assert(mappedNumVisibleMeshesPtr);

    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);

    GLSceneData sceneData(renderSettings.meshFile, renderSettings.sceneFile,
                          renderSettings.materialFile);

    GLMesh mesh(sceneData);

    SkyboxRenderer skybox("Resources/immenstadter_horn_2k.hdr",
                          "Resources/immenstadter_horn_2k_irradiance.hdr");
    ImGuiGLRenderer rendererUI;

    auto bufferIndirectMeshesOpaque = CreateIndirectBuffer(sceneData.shapes.size());
    auto bufferIndirectMeshesTransparent = CreateIndirectBuffer(sceneData.shapes.size());

    auto IsTransparent = [&](const DrawElementsIndirectCommand& c) {
        const auto mtlIndex = c.baseInstance & 0xffff;
        const auto& mtl = sceneData.materials[mtlIndex];
        return (mtl.flags & u32(MaterialFlags::TRANSPARENT)) > 0;
    };

    mesh.m_BufferIndirect->SelectDrawCommands(
        bufferIndirectMeshesOpaque,
        [&](const DrawElementsIndirectCommand& c) -> bool { return !IsTransparent(c); });
    mesh.m_BufferIndirect->SelectDrawCommands(
        bufferIndirectMeshesTransparent,
        [&](const DrawElementsIndirectCommand& c) -> bool { return IsTransparent(c); });

    LOG_INFO("Transparent meshes draw count: ",
             bufferIndirectMeshesTransparent->m_DrawCommands.size());
    LOG_INFO("Opaque meshes draw count: ", bufferIndirectMeshesOpaque->m_DrawCommands.size());

    int windowWidth;
    int windowHeight;
    glfwGetFramebufferSize(windowPtr, &windowWidth, &windowHeight);

    /*
     * Offscreen render targets.
     */

    /*
     * 2 color attachments for the main mesh pass: mesh itself(with forward shading) and
     * velocity TAA buffer.
     * XXX: Might want to add more texture params freedoms(such as sampling filter) for the
     *      framebuffer.
     */
    auto fbOpaque
        = CreateFramebuffer(windowWidth, windowHeight, GL_RGBA16F, GL_DEPTH_COMPONENT24, 2);
    fbOpaque->attachmentColor = fbOpaque->attachmentColors[0];

    auto fbLuminance = CreateFramebuffer(64, 64, GL_RGBA16F, 0);
    auto fbBrightPass = CreateFramebuffer(256, 256, GL_RGBA16F, 0);
    auto fbBloom1 = CreateFramebuffer(256, 256, GL_RGBA16F, 0);
    auto fbBloom2 = CreateFramebuffer(256, 256, GL_RGBA16F, 0);
    auto fbSSAO = CreateFramebuffer(1024, 1024, GL_RGBA8, 0);
    auto fbSSAOBlur = CreateFramebuffer(1024, 1024, GL_RGBA8, 0);

    auto fbShadowMap = CreateFramebuffer(8192, 8192, GL_R8, GL_DEPTH_COMPONENT24);
    const GLint swizzleMask[] = {GL_RED, GL_RED, GL_RED, GL_ONE};
    glTextureParameteriv(fbShadowMap->attachmentColor->m_Handle, GL_TEXTURE_SWIZZLE_RGBA,
                         swizzleMask);
    glTextureParameteriv(fbShadowMap->attachmentDepth->m_Handle, GL_TEXTURE_SWIZZLE_RGBA,
                         swizzleMask);

    // Last offscreen framebuffer for presenting.
    auto fbScreen = CreateFramebuffer(windowWidth, windowHeight, GL_RGBA16F, GL_DEPTH_COMPONENT24);

    // Tone mapping.
    auto fsToneMap = CreateShader("Shaders/PostProcess/ToneMap.fs.glsl");
    auto programToneMap = CreateProgram(vsFullScreenQuad, fsToneMap);
    auto fbPreToneMap = CreateFramebuffer(windowWidth, windowHeight, GL_RGBA16F, 0);

    /*
     * Textures.
     */
    // Texture view for last mip level (1x1 pixel) of luminance fbScreen->
    GLuint luminance1x1;
    glGenTextures(1, &luminance1x1);
    glTextureView(luminance1x1, GL_TEXTURE_2D, fbLuminance->attachmentColor->m_Handle, GL_RGBA16F,
                  6, 1, 0, 1);

    // Luminance textures for light adaptation.
    auto textureLuminance1 = CreateTexture(GL_TEXTURE_2D, 1, 1, GL_RGBA16F);
    auto textureLuminance2 = CreateTexture(GL_TEXTURE_2D, 1, 1, GL_RGBA16F);
    TextureHandle textureLuminances[] = {textureLuminance1, textureLuminance2};

    // First fbScreen->
    const vec4 brightPixel(vec3(50.0f), 1.0f);
    glTextureSubImage2D(textureLuminance1->m_Handle, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT,
                        glm::value_ptr(brightPixel));

    // SSAO rotation texture.
    auto textureRotationPattern = CreateTexture(GL_TEXTURE_2D, "Resources/rot_texture.bmp");

    // Transparency resources.
    const u32 MaxOITFragments = 16 * 1024 * 1024;
    const GLuint BufferIndex_TransparencyLists = BUFFER_INDEX_MATERIALS + 1;

    auto bufferOITAtomicCounter = CreateBuffer(sizeof(u32), nullptr, GL_DYNAMIC_STORAGE_BIT);
    auto bufferOITTransparencyLists = CreateBuffer(sizeof(GPUTransparentFragment) * MaxOITFragments,
                                                   nullptr, GL_DYNAMIC_STORAGE_BIT);
    auto textureOITHeads = CreateTexture(GL_TEXTURE_2D, windowWidth, windowHeight, GL_R32UI);

    glBindImageTexture(0, textureOITHeads->m_Handle, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, bufferOITAtomicCounter->m_Handle);

    auto ClearTransparencyBuffers = [&]() {
        const u32 minusOne = 0xFFFFFFFF;
        const u32 zero = 0;
        glClearTexImage(textureOITHeads->m_Handle, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, &minusOne);
        glNamedBufferSubData(bufferOITAtomicCounter->m_Handle, 0, sizeof(u32), &zero);
    };

    /*
     * Bounding boxes.
     */
    std::vector<BoundingBox> reorderedBoxes;
    reorderedBoxes.reserve(sceneData.shapes.size());

    // Transform bounding boxes to world space.
    for (const auto& drawData : sceneData.shapes)
    {
        const mat4 model = sceneData.scene.globalTransforms[drawData.transformIndex];
        reorderedBoxes.push_back(sceneData.meshData.boundingBoxes[drawData.meshIndex]);
        reorderedBoxes.back().Transform(model);
    }
    glNamedBufferSubData(bufferBoundingBoxes->m_Handle, 0,
                         reorderedBoxes.size() * sizeof(BoundingBox), reorderedBoxes.data());

    // Bounding box for the whole scene.
    BoundingBox wholeSceneBBox = reorderedBoxes.front();
    for (const auto& b : reorderedBoxes)
    {
        wholeSceneBBox.CombinePoint(b.min);
        wholeSceneBBox.CombinePoint(b.max);
    }

    // Sync flags.
    GLsync fenceCulling = nullptr;

    /*
     * TAA setup.
     */
    u32 jitterIndex = 0;
    u32 jitterSequenceCount = 8;

    auto fsTAAResolve = CreateShader("Shaders/AntiAliasing/TAAResolve.fs.glsl");
    auto programTAAResolve = CreateProgram(vsFullScreenQuad, fsTAAResolve);
    auto fsBlit = CreateShader("Shaders/AntiAliasing/Blit.fs.glsl");
    auto programBlit = CreateProgram(vsFullScreenQuad, fsBlit);

    auto fbTAAColor
        = CreateFramebuffer(windowWidth, windowHeight, GL_RGBA16F, GL_DEPTH_COMPONENT24);
    auto fbTAAColorHistory = CreateFramebuffer(windowWidth, windowHeight, GL_RGBA16F, 0);
    auto fbTaaDepthHistory = CreateFramebuffer(windowWidth, windowHeight, 0, GL_DEPTH_COMPONENT24);

    auto vsTAAMesh = CreateShader("Shaders/Scene/MeshTAA.vs.glsl");
    auto fsTAAMesh = CreateShader("Shaders/Scene/MeshTAA.fs.glsl");
    auto programTAAMesh = CreateProgram(vsTAAMesh, fsTAAMesh);

    auto bufferTAAParams = CreateBuffer(sizeof(GPUTAAParams), nullptr, GL_DYNAMIC_STORAGE_BIT);
    const u32 BufferIndex_TAAParams = 11;

    auto fsFXAA = CreateShader("Shaders/AntiAliasing/FXAA.fs.glsl");
    auto programFXAA = CreateProgram(vsFullScreenQuad, fsFXAA);

    /*
     * MSAA setup.
     */
    // auto fbMSAA = CreateFramebuffer(windowWidth, windowHeight, 0, GL_DEPTH_COMPONENT24);

    // u32 textureMSAAColorHandle;
    // glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &textureMSAAColorHandle);
    // glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, textureMSAAColorHandle);
    // glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGB, windowWidth, windowHeight,
    //                         GL_TRUE);
    // // glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
    // glBindFramebuffer(GL_FRAMEBUFFER, fbMSAA->m_Handle);
    // glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE,
    //                        textureMSAAColorHandle, 0);
    // // glNamedFramebufferTexture(fbMSAA->m_Handle, GL_COLOR_ATTACHMENT0, textureMSAAColorHandle,
    // 0);

    // fbMSAA->attachmentColor = TextureHandle::Create(new GLTexture(textureMSAAColorHandle));
    // fbMSAA->attachmentColors.push_back(fbMSAA->attachmentColor);
    // fbMSAA->SetDrawColorAttachments(0, 1);

    // const GLenum status1 = glCheckNamedFramebufferStatus(fbMSAA->m_Handle, GL_FRAMEBUFFER);
    // assert(status1 == GL_FRAMEBUFFER_COMPLETE);

    unsigned int framebuffer;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    // create a multisampled color attachment texture
    unsigned int textureColorBufferMultiSampled;
    glGenTextures(1, &textureColorBufferMultiSampled);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, textureColorBufferMultiSampled);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA16F, windowWidth, windowHeight,
                            GL_TRUE);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE,
                           textureColorBufferMultiSampled, 0);
    // create a (also multisampled) renderbuffer object for depth and stencil attachments
    unsigned int rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT24, windowWidth,
                                     windowHeight);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_COMPONENT24, GL_RENDERBUFFER, rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
        return -2;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Misc. utils.
    u32 frameCount = 0;
    FramesPerSecondCounter fpsCounter(0.5f);

    auto ImGuiPushFlagsAndStyles = [](bool value) {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, !value);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * value ? 1.0f : 0.2f);
    };
    auto ImGuiPopFlagsAndStyles = []() {
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
    };

    double timeStamp = glfwGetTime();
    float deltaSeconds = 0.0f;

    std::string gpuName = gpuDevice.GetGPUName();

    mat4 prevView;
    mat4 prevProj;

    LOG_INFO("Color att 0: ", fbOpaque->attachmentColors[0],
             " Handle: ", fbOpaque->attachmentColors[0]->m_Handle);
    LOG_INFO("Color att 1: ", fbOpaque->attachmentColors[1],
             " Handle: ", fbOpaque->attachmentColors[1]->m_Handle);

    float fov = 45.0f;
    float zNear = 0.1f;
    float zFar = 1000.0;
    bool depthTest = true;

    while (!glfwWindowShouldClose(windowPtr))
    {
        if (!nextFrame && stepMode)
        {
            window.PollEvents();
            window.SwapBuffers();
            continue;
        }
        nextFrame = false;

        const double newTimeStamp = glfwGetTime();
        deltaSeconds = static_cast<float>(newTimeStamp - timeStamp);
        timeStamp = newTimeStamp;

        fpsCounter.Tick(deltaSeconds);

        mainCameraController.Update(deltaSeconds, mouseState.pos, mouseState.pressedLeft);

        glfwGetFramebufferSize(windowPtr, &windowWidth, &windowHeight);
        const float ratio = windowWidth / (float)windowHeight;

        glViewport(0, 0, windowWidth, windowHeight);

        // Clear draw (opaque) framebuffer.
        glClearNamedFramebufferfv(fbOpaque->m_Handle, GL_COLOR, 0,
                                  glm::value_ptr(vec4(0.0f, 0.0f, 0.0f, 1.0f)));
        glClearNamedFramebufferfv(fbOpaque->m_Handle, GL_COLOR, 1,
                                  glm::value_ptr(vec4(0.0f, 0.0f, 0.0f, 1.0f)));
        glClearNamedFramebufferfi(fbOpaque->m_Handle, GL_DEPTH_STENCIL, 0, 1.0f, 0);

        glClearNamedFramebufferfv(fbScreen->m_Handle, GL_COLOR, 0,
                                  glm::value_ptr(vec4(0.0f, 0.0f, 0.0f, 1.0f)));
        glClearNamedFramebufferfi(fbScreen->m_Handle, GL_DEPTH_STENCIL, 0, 1.0f, 0);

        glClearNamedFramebufferfv(fbTAAColor->m_Handle, GL_COLOR, 0,
                                  glm::value_ptr(vec4(0.0f, 0.0f, 0.0f, 1.0f)));
        glClearNamedFramebufferfi(fbTAAColor->m_Handle, GL_DEPTH_STENCIL, 0, 1.0f, 0);

        glClearNamedFramebufferfv(framebuffer, GL_COLOR, 1,
                                  glm::value_ptr(vec4(0.0f, 0.0f, 0.0f, 1.0f)));
        glClearNamedFramebufferfi(framebuffer, GL_DEPTH_STENCIL, 0, 1.0f, 0);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const mat4 proj = glm::perspective(fov, ratio, zNear, zFar);
        const mat4 view = mainCamera.GetViewMatrix();

        if (frameCount == 0)
        {
            prevView = view;
            prevProj = proj;
        }

        // Light parameters for shadow mapping.
        const glm::mat4 rot1
            = glm::rotate(mat4(1.f), glm::radians(renderState.lightTheta), glm::vec3(0, 0, 1));
        const glm::mat4 rot2
            = glm::rotate(rot1, glm::radians(renderState.lightPhi), glm::vec3(1, 0, 0));
        const vec3 lightDir = glm::normalize(vec3(rot2 * vec4(0.0f, -1.0f, 0.0f, 1.0f)));
        const mat4 lightView = glm::lookAt(glm::vec3(0.0f), lightDir, vec3(0, 0, 1));
        const BoundingBox box = wholeSceneBBox.GetTransformed(lightView);
        const mat4 lightProj
            = glm::ortho(box.min.x, box.max.x, box.min.y, box.max.y, -box.max.z, -box.min.z);

        if (!renderState.freezeCullingView)
        {
            renderState.cullingView = mainCamera.GetViewMatrix();
        }

        GPUSceneData sceneData;
        sceneData.view = view;
        sceneData.proj = proj;
        sceneData.light = lightProj * lightView;
        sceneData.cameraPos = glm::vec4(mainCamera.GetPosition(), 1.0f);
        GetFrustumPlanes(proj * renderState.cullingView, sceneData.frustumPlanes);
        GetFrustumCorners(proj * renderState.cullingView, sceneData.frustumCorners);

        GPUPrevFrameData prevFrameData;
        prevFrameData.prevView = prevView;
        prevFrameData.prevProj = prevProj;
        prevFrameData.currViewWidth = windowWidth;
        prevFrameData.currViewHeight = windowHeight;
        glNamedBufferSubData(bufferPrevFrameData->m_Handle, 0, BufferSize_PrevFrameData,
                             &prevFrameData);

        ClearTransparencyBuffers();

        // Culling. XXX: Do not dispatch compute when GPU culling is not enabled.
        {
            *mappedNumVisibleMeshesPtr = 0;
            programCull->Use();
            glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BufferIndex_BoundingBoxes,
                             bufferBoundingBoxes->m_Handle);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BufferIndex_NumVisibleMeshes,
                             bufferNumVisibleMeshes->m_Handle);

            // Cull opaque meshes.
            sceneData.numShapesToCull = renderState.enableGPUCulling
                                            ? (u32)bufferIndirectMeshesOpaque->m_DrawCommands.size()
                                            : 0u;
            glNamedBufferSubData(bufferSceneData->m_Handle, 0, BufferSize_SceneData, &sceneData);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BufferIndex_DrawCommands,
                             bufferIndirectMeshesOpaque->m_Handle);
            glDispatchCompute(1 + (GLuint)bufferIndirectMeshesOpaque->m_DrawCommands.size() / 64, 1,
                              1);

            // Cull transparent meshes.
            sceneData.numShapesToCull
                = renderState.enableGPUCulling
                      ? (u32)bufferIndirectMeshesTransparent->m_DrawCommands.size()
                      : 0u;
            glNamedBufferSubData(bufferSceneData->m_Handle, 0, BufferSize_SceneData, &sceneData);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BufferIndex_DrawCommands,
                             bufferIndirectMeshesTransparent->m_Handle);
            glDispatchCompute(
                1 + (GLuint)bufferIndirectMeshesTransparent->m_DrawCommands.size() / 64, 1, 1);

            glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT
                            | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
            fenceCulling = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        }

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BufferIndex_TransparencyLists,
                         bufferOITTransparencyLists->m_Handle);

        // Shadow map (depth) pass.
        if (renderState.enableShadows)
        {
            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);

            const GPUSceneData sceneDataShadows = {
                .view = lightView,
                .proj = lightProj,
            };
            glNamedBufferSubData(bufferSceneData->m_Handle, 0, BufferSize_SceneData,
                                 &sceneDataShadows);

            glClearNamedFramebufferfv(fbShadowMap->m_Handle, GL_COLOR, 0,
                                      glm::value_ptr(vec4(0.0f, 0.0f, 0.0f, 1.0f)));
            glClearNamedFramebufferfi(fbShadowMap->m_Handle, GL_DEPTH_STENCIL, 0, 1.0f, 0);

            fbShadowMap->Bind();
            programShadowMap->Use();
            mesh.Draw(mesh.m_BufferIndirect->m_DrawCommands.size());
            fbShadowMap->Unbind();

            // Setup shadow light for whole scene.
            sceneData.light = lightProj * lightView;
            glBindTextureUnit(4, fbShadowMap->attachmentDepth->m_Handle);
        }
        else
        {
            // Disable shadows. XXX: Have a proper flag for this and replace this trick.
            sceneData.light = mat4(0.0f);
        }

        // TAA jitter.
        float haltonX = 2.0f * Halton(jitterIndex + 1, 2) - 1.0f;
        float haltonY = 2.0f * Halton(jitterIndex + 1, 3) - 1.0f;
        float jitterX = (haltonX / windowWidth);
        float jitterY = (haltonY / windowHeight);

        jitterIndex = (jitterIndex + 1) % taaParams.haltonSequenceCount;

        sceneData.jitterOffsetX = jitterX;
        sceneData.jitterOffsetY = jitterY;

        glNamedBufferSubData(bufferSceneData->m_Handle, 0, BufferSize_SceneData, &sceneData);

        // Mesh pass.
        {
            glDisable(GL_BLEND);

            if (depthTest)
            {
                glEnable(GL_DEPTH_TEST);
            }
            else
            {
                glDisable(GL_DEPTH_TEST);
            }

            // if (renderState.enableMSAA_4X)
            // {
            //     // fbMSAA->Bind();S
            //     glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
            // }
            // else
            {
                fbOpaque->Bind();
            }

            fbOpaque->SetDrawColorAttachments(0, 1);
            skybox.Draw();

            if (renderState.drawOpaque)
            {
                if (renderState.enableTAA)
                {
                    // programSceneMesh->Use();
                    programTAAMesh->Use();
                    // programSceneMeshNoJitter->Use();
                    fbOpaque->SetDrawColorAttachments(0, 2);
                }
                else
                {
                    programSceneMeshNoJitter->Use();
                }

                mesh.Draw(bufferIndirectMeshesOpaque->m_DrawCommands.size(),
                          bufferIndirectMeshesOpaque);
            }
            if (renderState.drawTransparent)
            {
                fbOpaque->SetDrawColorAttachments(0, 1);

                glBindImageTexture(0, textureOITHeads->m_Handle, 0, GL_FALSE, 0, GL_READ_WRITE,
                                   GL_R32UI);
                glDepthMask(GL_FALSE);
                glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

                // XXX: Currently transparent meshes not sampled (no TAA jitter).
                programOITMesh->Use();
                mesh.Draw(bufferIndirectMeshesTransparent->m_DrawCommands.size(),
                          bufferIndirectMeshesTransparent);

                // This shader pass is needed, even with no draw commands.
                // mesh.Draw(bufferIndirectMeshesTransparent->m_DrawCommands.size(), 0);

                glFlush();
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
                glDepthMask(GL_TRUE);
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            }

            // if (renderState.enableMSAA_4X)
            // {
            //     // fbMSAA->Unbind();
            //     // glBindFramebuffer(GL_FRAMEBUFFER, 0);

            //     // fbOpaque->Bind();
            //     // programBlit->Use();
            //     // glBindTextureUnit(0, textureColorBufferMultiSampled);
            //     // glDrawArrays(GL_TRIANGLES, 0, 6);
            //     // fbOpaque->Unbind();

            //     // glBlitNamedFramebuffer(framebuffer, 0, 0, 0, windowWidth, windowHeight, 0, 0,
            //     //                        windowWidth, windowHeight, GL_COLOR_BUFFER_BIT,
            //     //                        GL_LINEAR);

            //     glBlitNamedFramebuffer(framebuffer, fbOpaque->m_Handle, 0, 0, windowWidth,
            //                            windowHeight, 0, 0, windowWidth, windowHeight,
            //                            GL_DEPTH_BUFFER_BIT, GL_NEAREST);
            // }
            // else
            {
                fbOpaque->Unbind();
            }
        }

        glDisable(GL_DEPTH_TEST);

        // SSAO.
        if (renderState.enableSSAO)
        {
            glClearNamedFramebufferfv(fbSSAO->m_Handle, GL_COLOR, 0,
                                      glm::value_ptr(vec4(0.0f, 0.0f, 0.0f, 1.0f)));
            glNamedBufferSubData(bufferSceneData->m_Handle, 0, sizeof(ssaoParams), &ssaoParams);

            fbSSAO->Bind();
            programSSAO->Use();
            glBindTextureUnit(0, fbOpaque->attachmentDepth->m_Handle);
            glBindTextureUnit(1, textureRotationPattern->m_Handle);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            fbSSAO->Unbind();

            if (renderState.enableSSAOBlur)
            {
                // Blur X. Render to blur fbScreen->
                fbSSAOBlur->Bind();
                programBlurX->Use();
                glBindTextureUnit(0, fbSSAO->attachmentColor->m_Handle);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                fbSSAOBlur->Unbind();

                // Blur Y. Re-target to SSAO framebuffer again.
                fbSSAO->Bind();
                programBlurY->Use();
                glBindTextureUnit(0, fbSSAOBlur->attachmentColor->m_Handle);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                fbSSAO->Unbind();
            }
            glClearNamedFramebufferfv(fbScreen->m_Handle, GL_COLOR, 0,
                                      glm::value_ptr(vec4(0.0f, 0.0f, 0.0f, 1.0f)));
            fbScreen->Bind();
            programSSAOCombine->Use();
            glBindTextureUnit(0, fbOpaque->attachmentColor->m_Handle);
            glBindTextureUnit(1, fbSSAO->attachmentColor->m_Handle);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            fbScreen->Unbind();
        }
        else
        {
            glClearNamedFramebufferfv(fbScreen->m_Handle, GL_COLOR, 0,
                                      glm::value_ptr(vec4(0.0f, 0.0f, 0.0f, 1.0f)));

            fbScreen->Bind();
            programBlit->Use();
            glBindTextureUnit(0, fbOpaque->attachmentColor->m_Handle);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            fbScreen->Unbind();
        }

        // Combine Transparent/OIT meshes.
        {
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);

            fbOpaque->Bind();
            fbOpaque->SetDrawColorAttachments(0, 1);

            programOIT->Use();
            glBindTextureUnit(0, fbScreen->attachmentColor->m_Handle);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            fbOpaque->Unbind();
            glBlitNamedFramebuffer(fbOpaque->m_Handle, fbScreen->m_Handle, 0, 0, windowWidth,
                                   windowHeight, 0, 0, windowWidth, windowHeight,
                                   GL_COLOR_BUFFER_BIT, GL_LINEAR);
        }

        // // TAA.
        // if (renderState.enableTAA)
        // {
        //     // Copy current color buffer to history in first frame.
        //     if (frameCount == 0)
        //     {
        //         fbTAAColorHistory->Bind();
        //         programBlit->Use();
        //         glBindTextureUnit(0, fbScreen->attachmentColor->m_Handle);
        //         glDrawArrays(GL_TRIANGLES, 0, 6);
        //         fbTAAColorHistory->Unbind();

        //         glBlitNamedFramebuffer(fbOpaque->m_Handle,
        //                                fbTaaDepthHistory->attachmentDepth->m_Handle, 0, 0,
        //                                windowWidth, windowHeight, 0, 0, windowWidth,
        //                                windowHeight, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        //     }

        //     // XXX: Does current color buffer need to be sampled nearest?
        //     // glTextureParameteri(fbScreen->attachmentColor->m_Handle,
        //     GL_TEXTURE_MIN_FILTER,
        //     //                     GL_NEAREST);
        //     // glTextureParameteri(fbScreen->attachmentColor->m_Handle,
        //     GL_TEXTURE_MAG_FILTER,
        //     //                     GL_NEAREST);

        //     // Resolve TAA.
        //     fbTAAColor->Bind();
        //     programTAAResolve->Use();
        //     glBindTextureUnit(0, fbScreen->attachmentColor->m_Handle);
        //     // Bind history.
        //     glBindTextureUnit(1, fbTAAColorHistory->attachmentColor->m_Handle);
        //     // Bind velocity.
        //     glBindTextureUnit(2, fbOpaque->attachmentColors[1]->m_Handle);
        //     // Bind depth.
        //     glBindTextureUnit(3, fbOpaque->attachmentDepth->m_Handle);
        //     // Bind depth history.
        //     glBindTextureUnit(4, fbTaaDepthHistory->attachmentDepth->m_Handle);
        //     // glBindTextureUnit(4, fbOpaque->attachmentDepth->m_Handle);

        //     glDrawArrays(GL_TRIANGLES, 0, 6);
        //     fbTAAColor->Unbind();

        //     // Copy to history buffer.
        //     fbTAAColorHistory->Bind();
        //     programBlit->Use();
        //     glBindTextureUnit(0, fbTAAColor->attachmentColor->m_Handle);
        //     glDrawArrays(GL_TRIANGLES, 0, 6);
        //     fbTAAColorHistory->Unbind();

        //     glBlitNamedFramebuffer(fbOpaque->m_Handle, fbTaaDepthHistory->m_Handle, 0, 0,
        //                            windowWidth, windowHeight, 0, 0, windowWidth,
        //                            windowHeight, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        // }
        // else
        // {
        //     glBlitNamedFramebuffer(fbScreen->m_Handle, fbTAAColor->m_Handle, 0, 0,
        //     windowWidth,
        //                            windowHeight, 0, 0, windowWidth, windowHeight,
        //                            GL_COLOR_BUFFER_BIT, GL_LINEAR);
        // }
        // glBlitNamedFramebuffer(fbTAAColor->m_Handle, fbScreen->m_Handle, 0, 0, windowWidth,
        //                        windowHeight, 0, 0, windowWidth, windowHeight,
        //                        GL_COLOR_BUFFER_BIT, GL_LINEAR);

        // HDR.
        if (renderState.enableHDR)
        {
            glNamedBufferSubData(bufferSceneData->m_Handle, 0, sizeof(hdrParams), &hdrParams);

            // Downscale and convert.
            fbLuminance->Bind();
            programLuminance->Use();
            glBindTextureUnit(0, fbScreen->attachmentColor->m_Handle);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            fbLuminance->Unbind();
            glGenerateTextureMipmap(fbLuminance->attachmentColor->m_Handle);

            // Light Adaptation.
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
            programAdaptation->Use();
            glBindImageTexture(0, textureLuminances[0]->m_Handle, 0, GL_TRUE, 0, GL_READ_ONLY,
                               GL_RGBA16F);
            glBindImageTexture(1, luminance1x1, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA16F);
            glBindImageTexture(2, textureLuminances[1]->m_Handle, 0, GL_TRUE, 0, GL_WRITE_ONLY,
                               GL_RGBA16F);
            glDispatchCompute(1, 1, 1);
            glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

            // Bright pass: extract bright areas.
            fbBrightPass->Bind();
            programBrightPass->Use();
            glBindTextureUnit(0, fbScreen->attachmentColor->m_Handle);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            fbBrightPass->Unbind();
            glBlitNamedFramebuffer(fbBrightPass->m_Handle, fbBloom2->m_Handle, 0, 0, 256, 256, 0, 0,
                                   256, 256, GL_COLOR_BUFFER_BIT, GL_LINEAR);

            // Blur.
            for (int i = 0; i != 4; i++)
            {
                // 2.4 Blur X
                fbBloom1->Bind();
                programBlurX->Use();
                glBindTextureUnit(0, fbBloom2->attachmentColor->m_Handle);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                fbBloom1->Unbind();
                // 2.5 Blur Y
                fbBloom2->Bind();
                programBlurY->Use();
                glBindTextureUnit(0, fbBloom1->attachmentColor->m_Handle);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                fbBloom2->Unbind();
            }

            fbPreToneMap->Bind();
            programHDRCombine->Use();
            glBindTextureUnit(0, fbScreen->attachmentColor->m_Handle);
            glBindTextureUnit(1, textureLuminances[1]->m_Handle);
            glBindTextureUnit(2, fbBloom2->attachmentColor->m_Handle);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            fbPreToneMap->Unbind();
        }
        else
        {
            glBlitNamedFramebuffer(fbScreen->m_Handle, fbPreToneMap->m_Handle, 0, 0, windowWidth,
                                   windowHeight, 0, 0, windowWidth, windowHeight,
                                   GL_COLOR_BUFFER_BIT, GL_LINEAR);
        }

        // TAA.
        if (renderState.enableTAA)
        {
            // Copy current color buffer to history in first frame.
            if (frameCount == 0)
            {
                fbTAAColorHistory->Bind();
                programBlit->Use();
                glBindTextureUnit(0, fbPreToneMap->attachmentColor->m_Handle);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                fbTAAColorHistory->Unbind();

                glBlitNamedFramebuffer(fbOpaque->m_Handle, fbTaaDepthHistory->m_Handle, 0, 0,
                                       windowWidth, windowHeight, 0, 0, windowWidth, windowHeight,
                                       GL_DEPTH_BUFFER_BIT, GL_NEAREST);
            }

            // XXX: Does current color buffer need to be sampled nearest?
            // glTextureParameteri(fbPreToneMap->attachmentColor->m_Handle,
            // GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            // glTextureParameteri(fbPreToneMap->attachmentColor->m_Handle,
            // GL_TEXTURE_MAG_FILTER, GL_NEAREST);

            glNamedBufferSubData(bufferTAAParams->m_Handle, 0, sizeof(taaParams), &taaParams);
            glBindBufferBase(GL_UNIFORM_BUFFER, BufferIndex_TAAParams, bufferTAAParams->m_Handle);

            // Resolve TAA.
            fbTAAColor->Bind();
            programTAAResolve->Use();
            // programFXAA->Use();
            glBindTextureUnit(0, fbPreToneMap->attachmentColor->m_Handle);
            // Bind history.
            glBindTextureUnit(1, fbTAAColorHistory->attachmentColor->m_Handle);
            // Bind velocity.
            glBindTextureUnit(2, fbOpaque->attachmentColors[1]->m_Handle);

            glBindTextureUnit(3, fbOpaque->attachmentDepth->m_Handle);
            glBindTextureUnit(4, fbTaaDepthHistory->attachmentDepth->m_Handle);

            glBindTextureUnit(5, textureLuminances[1]->m_Handle);
            glBindTextureUnit(6, textureLuminances[1]->m_Handle);

            glDrawArrays(GL_TRIANGLES, 0, 6);
            fbTAAColor->Unbind();

            // Copy to history buffer.
            fbTAAColorHistory->Bind();
            programBlit->Use();
            glBindTextureUnit(0, fbTAAColor->attachmentColor->m_Handle);
            // glBindTextureUnit(0, fbPreToneMap->attachmentColor->m_Handle);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            fbTAAColorHistory->Unbind();

            glBlitNamedFramebuffer(fbOpaque->m_Handle, fbTaaDepthHistory->m_Handle, 0, 0,
                                   windowWidth, windowHeight, 0, 0, windowWidth, windowHeight,
                                   GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        }
        else if (renderState.enableFXAA)
        {
            fbTAAColor->Bind();
            programFXAA->Use();
            glBindTextureUnit(0, fbPreToneMap->attachmentColor->m_Handle);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            fbTAAColor->Unbind();
        }
        else
        {
            glBlitNamedFramebuffer(fbPreToneMap->m_Handle, fbTAAColor->m_Handle, 0, 0, windowWidth,
                                   windowHeight, 0, 0, windowWidth, windowHeight,
                                   GL_COLOR_BUFFER_BIT, GL_LINEAR);
        }

        // Tone mapping.
        {
            // Bind to swapchain buffer.
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, windowWidth, windowHeight);

            programToneMap->Use();
            glBindTextureUnit(0, fbTAAColor->attachmentColor->m_Handle);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        // Compute synchronization.
        if (renderState.enableGPUCulling && fenceCulling)
        {
            for (;;)
            {
                const GLenum res = glClientWaitSync(fenceCulling, GL_SYNC_FLUSH_COMMANDS_BIT, 1000);
                if (res == GL_ALREADY_SIGNALED || res == GL_CONDITION_SATISFIED)
                    break;
            }
            glDeleteSync(fenceCulling);
        }

        glViewport(0, 0, windowWidth, windowHeight);

        const float indentSize = 16.0f;
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2((float)windowWidth, (float)windowHeight);
        ImGui::NewFrame();
        ImGui::Begin("Render Control", nullptr);

        ImGui::Text("Shadows");
        ImGui::Indent(indentSize);
        ImGui::Checkbox("Enable Shadows", &renderState.enableShadows);
        ImGuiPushFlagsAndStyles(renderState.enableShadows);
        ImGui::SliderFloat("Light Angle 1", &renderState.lightTheta, -85.0f, +85.0f);
        ImGui::SliderFloat("Light Angle 2", &renderState.lightPhi, -85.0f, +85.0f);
        ImGuiPopFlagsAndStyles();
        ImGui::Unindent(indentSize);
        ImGui::Separator();

        ImGui::Text("Transparency");
        ImGui::Indent(indentSize);
        ImGui::Checkbox("Opaque Meshes", &renderState.drawOpaque);
        ImGui::Checkbox("Transparent Meshes", &renderState.drawTransparent);
        ImGui::Unindent(indentSize);
        ImGui::Separator();

        ImGui::Text("Culling");
        ImGui::Indent(indentSize);
        ImGui::Checkbox("Enable Cull", &renderState.enableGPUCulling);
        ImGuiPushFlagsAndStyles(renderState.enableGPUCulling);
        ImGui::Checkbox("Freeze Culling", &renderState.freezeCullingView);
        ImGui::Text("Visible Mesh Count: %i", *mappedNumVisibleMeshesPtr);
        ImGuiPopFlagsAndStyles();
        ImGui::Unindent(indentSize);
        ImGui::Separator();

        ImGui::Text("SSAO");
        ImGui::Indent(indentSize);
        ImGui::Checkbox("Enable SSAO", &renderState.enableSSAO);
        ImGuiPushFlagsAndStyles(renderState.enableSSAO);
        ImGui::Checkbox("Enable SSAO Blur", &renderState.enableSSAOBlur);
        ImGui::SliderFloat("SSAO Scale", &ssaoParams.scale, 0.0f, 2.0f);
        ImGui::SliderFloat("SSAO Bias", &ssaoParams.bias, 0.0f, 0.3f);
        ImGui::SliderFloat("SSAO Radius", &ssaoParams.radius, 0.02f, 0.2f);
        ImGui::SliderFloat("SSAO Attenuation scale", &ssaoParams.attScale, 0.5f, 1.5f);
        ImGui::SliderFloat("SSAO Distance scale", &ssaoParams.distScale, 0.0f, 1.0f);
        ImGuiPopFlagsAndStyles();
        ImGui::Unindent(indentSize);
        ImGui::Separator();

        ImGui::Text("HDR");
        ImGui::Indent(indentSize);
        ImGui::Checkbox("Enable HDR", &renderState.enableHDR);
        ImGuiPushFlagsAndStyles(renderState.enableHDR);
        ImGui::SliderFloat("Exposure", &hdrParams.exposure, 0.1f, 2.0f);
        ImGui::SliderFloat("Max White", &hdrParams.maxWhite, 0.5f, 2.0f);
        ImGui::SliderFloat("Bloom Strength", &hdrParams.bloomStrength, 0.0f, 2.0f);
        ImGui::SliderFloat("Adaptation Speed", &hdrParams.adaptationSpeed, 0.01f, 0.5f);
        ImGuiPopFlagsAndStyles();
        ImGui::Unindent(indentSize);
        ImGui::Separator();

        ImGui::Text("Anti-Aliasing");
        ImGui::Indent(indentSize);

        ImGui::Checkbox("Enable TAA", &renderState.enableTAA);
        ImGuiPushFlagsAndStyles(renderState.enableTAA);
        ImGui::SliderInt("Jitter Sequence Count", (int*)&taaParams.haltonSequenceCount, 8, 64);
        ImGui::SliderFloat("Source Weight", &taaParams.sourceWeight, 0.0f, 2.0f);
        ImGui::SliderInt("Color Clamping Type", (int*)&taaParams.colorClampingType, 0, 5);
        ImGui::SliderInt("Luminance Weighing", (int*)&taaParams.luminanceWeigh, 0, 5);
        ImGuiPopFlagsAndStyles();

        ImGui::Checkbox("Enable FXAA", &renderState.enableFXAA);
        ImGuiPushFlagsAndStyles(renderState.enableFXAA);
        ImGui::SliderFloat("FXAA Threshold", &fxaaParams.threshold, 0.01f, 0.06f);
        ImGui::SliderFloat("FXAA Relative Threshold", &taaParams.sourceWeight, 0.05f, 0.3f);
        ImGuiPopFlagsAndStyles();

        ImGui::Checkbox("Enable MSAA 4X", &renderState.enableMSAA_4X);

        ImGui::Unindent(indentSize);
        ImGui::Separator();

        ImGui::Text("Misc.");
        ImGui::Indent(indentSize);
        ImGui::Checkbox("Enable Depth Test", &depthTest);
        ImGui::SliderFloat("FOV", &fov, 45.0f, 90.0f);
        ImGui::SliderFloat("Near Z", &zNear, 0.001, 10.0f);
        ImGui::SliderFloat("Far Z", &zFar, 20.0f, 10000.0f);
        ImGui::Unindent(indentSize);
        ImGui::Separator();

        ImGui::End();

        if (renderState.enableSSAO)
            ImguiTextureWindowGL("SSAO", fbSSAO->attachmentColor->m_Handle);
        // if (renderState.enableShadows)
        //     ImguiTextureWindowGL("Shadow Map", fbShadowMap->attachmentDepth->m_Handle);

        // // ImguiTextureWindowGL("Depth Map", fbOpaque->attachmentDepth->m_Handle);

        // ImguiTextureWindowGL("MSAA Mesh Buffer", textureColorBufferMultiSampled);

        ImguiTextureWindowGL("Velocity Buffer", fbOpaque->attachmentColors[1]->m_Handle);

        // ImguiTextureWindowGL("History Depth Buffer",
        // fbTaaDepthHistory->attachmentDepth->m_Handle); ImguiTextureWindowGL("Depth Buffer",
        // fbOpaque->attachmentDepth->m_Handle);

        ImguiTextureWindowGL("Pre Tonemap Buffer", fbPreToneMap->attachmentColor->m_Handle);
        ImguiTextureWindowGL("Anti Aliased Buffer", fbTAAColor->attachmentColor->m_Handle);

        // ImguiTextureWindowGL("Bright pass", fbBrightPass->attachmentColor->m_Handle);

        const ImGuiWindowFlags guiflags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
                                          | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar
                                          | ImGuiWindowFlags_NoSavedSettings
                                          | ImGuiWindowFlags_NoInputs;
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::Begin("Information", nullptr, guiflags);
        ImGui::Text("GPU: %s", gpuName.c_str());
        ImGui::Text("FPS: %f", fpsCounter.GetFPS());
        ImGui::Text("Frame number: %d", frameCount);
        ImGui::End();

        ImGui::Render();
        rendererUI.Render(windowWidth, windowHeight, ImGui::GetDrawData());

        // Swap luminance textures.
        textureLuminances[0].Swap(textureLuminances[1]);

        window.PollEvents();
        window.SwapBuffers();
        frameCount++;

        prevView = sceneData.view;
        prevProj = sceneData.proj;
    }

    glUnmapNamedBuffer(bufferNumVisibleMeshes->m_Handle);
    glDeleteTextures(1, &luminance1x1);
    // glDeleteTextures(1, &textureMSAAColorHandle);

    return 0;
}
