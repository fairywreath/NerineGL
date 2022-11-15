#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4099) // type name first seen using 'class' now seen using 'struct'
#pragma warning(                                                                                   \
    disable : 4244) // 'argument': conversion from 'float' to 'int', possible loss of data
#pragma warning(disable : 4267) // conversion from 'size_t' to 'int', possible loss of data
#pragma warning(disable : 4305) // 'argument': truncation from 'double' to 'float'
#endif                          // _MSC_VER

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif // __clang__

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "imgui.cpp"
#include "imgui_demo.cpp"
#include "imgui_draw.cpp"
#include "imgui_tables.cpp"
#include "imgui_widgets.cpp"

#if defined(_MSC_VER)
#pragma warning(pop)
#endif // _MSC_VER

#include "GLImGui.h"

const GLchar* shaderCodeImGuiVertex = R"(
	#version 460 core
	layout (location = 0) in vec2 Position;
	layout (location = 1) in vec2 UV;
	layout (location = 2) in vec4 Color;
	layout(std140, binding = 7) uniform PerFrameData
	{
		uniform mat4 MVP;
	};
	out vec2 Frag_UV;
	out vec4 Frag_Color;
	void main()
	{
		Frag_UV = UV;
		Frag_Color = Color;
		gl_Position = MVP * vec4(Position.xy,0,1);
	}
)";

const GLchar* shaderCodeImGuiFragment = R"(
	#version 460 core
	in vec2 Frag_UV;
	in	vec4 Frag_Color;
	layout (binding = 0) uniform sampler2D Texture;
	layout (location = 0) out vec4 out_FragColor;
	void main()
	{
		out_FragColor = Frag_Color * texture(Texture, Frag_UV.st);
	}
)";

Nerine::ImGuiGLRenderer::ImGuiGLRenderer()
    : m_VS(CreateShader(GL_VERTEX_SHADER, shaderCodeImGuiVertex)),
      m_FS(CreateShader(GL_FRAGMENT_SHADER, shaderCodeImGuiFragment)),
      m_Program(CreateProgram(m_VS, m_FS))
{
    glCreateVertexArrays(1, &m_VAO);

    glVertexArrayElementBuffer(m_VAO, m_BufferElements->m_Handle);
    glVertexArrayVertexBuffer(m_VAO, 0, m_BufferVertices->m_Handle, 0, sizeof(ImDrawVert));

    glEnableVertexArrayAttrib(m_VAO, 0);
    glEnableVertexArrayAttrib(m_VAO, 1);
    glEnableVertexArrayAttrib(m_VAO, 2);

    glVertexArrayAttribFormat(m_VAO, 0, 2, GL_FLOAT, GL_FALSE, IM_OFFSETOF(ImDrawVert, pos));
    glVertexArrayAttribFormat(m_VAO, 1, 2, GL_FLOAT, GL_FALSE, IM_OFFSETOF(ImDrawVert, uv));
    glVertexArrayAttribFormat(m_VAO, 2, 4, GL_UNSIGNED_BYTE, GL_TRUE, IM_OFFSETOF(ImDrawVert, col));

    glVertexArrayAttribBinding(m_VAO, 0, 0);
    glVertexArrayAttribBinding(m_VAO, 1, 0);
    glVertexArrayAttribBinding(m_VAO, 2, 0);

    glBindBufferBase(GL_UNIFORM_BUFFER, 7, m_BufferPerFrameData->m_Handle);

    InitImGui();
}

Nerine::ImGuiGLRenderer::~ImGuiGLRenderer()
{
    glDeleteVertexArrays(1, &m_VAO);
    glDeleteTextures(1, &m_Texture);
}

void Nerine::ImGuiGLRenderer::Render(int width, int height, const ImDrawData* drawData)
{

    if (!drawData)
        return;

    glBindTextures(0, 1, &m_Texture);
    glBindVertexArray(m_VAO);
    m_Program->Use();

    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);

    const float L = drawData->DisplayPos.x;
    const float R = drawData->DisplayPos.x + drawData->DisplaySize.x;
    const float T = drawData->DisplayPos.y;
    const float B = drawData->DisplayPos.y + drawData->DisplaySize.y;
    const glm::mat4 orthoProjection = glm::ortho(L, R, B, T);

    glNamedBufferSubData(m_BufferPerFrameData->m_Handle, 0, sizeof(glm::mat4),
                         glm::value_ptr(orthoProjection));

    for (int n = 0; n < drawData->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = drawData->CmdLists[n];
        glNamedBufferSubData(m_BufferVertices->m_Handle, 0,
                             (GLsizeiptr)cmd_list->VtxBuffer.Size * sizeof(ImDrawVert),
                             cmd_list->VtxBuffer.Data);
        glNamedBufferSubData(m_BufferElements->m_Handle, 0,
                             (GLsizeiptr)cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx),
                             cmd_list->IdxBuffer.Data);

        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            const ImVec4 cr = pcmd->ClipRect;
            glScissor((int)cr.x, (int)(height - cr.w), (int)(cr.z - cr.x), (int)(cr.w - cr.y));
            glBindTextureUnit(0, (GLuint)(intptr_t)pcmd->TextureId);
            glDrawElementsBaseVertex(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, GL_UNSIGNED_SHORT,
                                     (void*)(intptr_t)(pcmd->IdxOffset * sizeof(ImDrawIdx)),
                                     (GLint)pcmd->VtxOffset);
        }
    }

    glScissor(0, 0, width, height);
    glDisable(GL_SCISSOR_TEST);
}

void Nerine::ImGuiGLRenderer::InitImGui()
{
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    // Build texture atlas.
    ImFontConfig cfg = ImFontConfig();
    cfg.FontDataOwnedByAtlas = false;
    cfg.RasterizerMultiply = 1.5f;
    cfg.SizePixels = 768.0f / 32.0f;
    cfg.PixelSnapH = true;
    cfg.OversampleH = 4;
    cfg.OversampleV = 4;
    ImFont* Font
        = io.Fonts->AddFontFromFileTTF("Resources/OpenSans-Light.ttf", cfg.SizePixels, &cfg);

    unsigned char* pixels = nullptr;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    glCreateTextures(GL_TEXTURE_2D, 1, &m_Texture);
    glTextureParameteri(m_Texture, GL_TEXTURE_MAX_LEVEL, 0);
    glTextureParameteri(m_Texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(m_Texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureStorage2D(m_Texture, 1, GL_RGBA8, width, height);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTextureSubImage2D(m_Texture, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    io.Fonts->TexID = (ImTextureID)(intptr_t)m_Texture;
    io.FontDefault = Font;
    io.DisplayFramebufferScale = ImVec2(1, 1);
}

void Nerine::ImguiTextureWindowGL(const std::string& title, u32 texId)
{
    ImGui::Begin(title.c_str(), nullptr);

    const ImVec2 vMin = ImGui::GetWindowContentRegionMin();
    const ImVec2 vMax = ImGui::GetWindowContentRegionMax();

    ImGui::Image((void*)(intptr_t)texId, ImVec2(vMax.x - vMin.x, vMax.y - vMin.y),
                 ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

    ImGui::End();
}
