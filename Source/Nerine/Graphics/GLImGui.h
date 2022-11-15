#pragma once

#include "GLResources.h"

#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_internal.h>

namespace Nerine
{

class ImGuiGLRenderer
{
public:
    ImGuiGLRenderer();
    ~ImGuiGLRenderer();

    NON_COPYABLE(ImGuiGLRenderer);
    NON_MOVEABLE(ImGuiGLRenderer);

    void Render(int width, int height, const ImDrawData* drawData);

private:
    void InitImGui();

private:
    GLuint m_Texture{0};
    GLuint m_VAO{0};

    BufferHandle m_BufferVertices{CreateBuffer(128 * 1024, nullptr, GL_DYNAMIC_STORAGE_BIT)};
    BufferHandle m_BufferElements{CreateBuffer(256 * 1024, nullptr, GL_DYNAMIC_STORAGE_BIT)};
    BufferHandle m_BufferPerFrameData{CreateBuffer(sizeof(mat4), nullptr, GL_DYNAMIC_STORAGE_BIT)};

    ShaderHandle m_VS;
    ShaderHandle m_FS;

    ProgramHandle m_Program;
};

void ImguiTextureWindowGL(const std::string& title, u32 texId);

} // namespace Nerine