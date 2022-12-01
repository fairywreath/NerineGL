#pragma once

#include <glad/glad.h>

#include <memory>
#include <string>
#include <vector>

#include <Core/Resource.h>
#include <Core/Types.h>

namespace Nerine
{

class GLBuffer : public RefCountResource<IResource>
{
public:
    GLBuffer() = default;
    GLBuffer(GLsizeiptr size, const void* data, GLbitfield flags);

    ~GLBuffer();

    void Create(GLsizeiptr size, const void* data, GLbitfield flags);
    void Destroy();

    GLuint m_Handle{0};
};

using BufferHandle = RefCountPtr<GLBuffer>;

BufferHandle CreateBuffer(GLsizeiptr size, const void* data, GLbitfield flags);

class GLTexture : public RefCountResource<IResource>
{
public:
    GLTexture() = default;

    GLTexture(GLenum type, u32 width, u32 height, GLenum internalFormat);

    GLTexture(GLenum type, const std::string& fileName, GLenum clamp = GL_REPEAT);
    GLTexture(u32 width, u32 height, const void* data);

    ~GLTexture();

    void Create(GLenum type, u32 width, u32 height, GLenum internalFormat,
                GLenum clamp = GL_REPEAT);
    void Write2D(u32 width, u32 height, const void* data);

    void Load(GLenum type, const std::string& fileName, GLenum clamp = GL_REPEAT);

    void Destroy();

    GLuint m_Handle{0};
    GLuint64 m_HandleBindless{0};

    u32 m_Width{0};
    u32 m_Height{0};
    GLenum m_Type{GL_INVALID_VALUE};
};

using TextureHandle = RefCountPtr<GLTexture>;

TextureHandle CreateTexture(GLenum type, const std::string& fileName, GLenum clamp = GL_REPEAT);
TextureHandle CreateTexture(GLenum type, u32 width, u32 height, GLenum internalFormat);
TextureHandle CreateTexture2D(u32 width, u32 height, const void* data);

class GLShader : public RefCountResource<IResource>
{
public:
    GLShader() = default;
    explicit GLShader(const std::string& fileName);
    GLShader(GLenum stage, const std::string& text, const std::string& debugName = "");

    ~GLShader();

    void Create(const std::string& fileName);
    void Create(GLenum stage, const std::string& text, const std::string& debugName = "");

    void Destroy();

    GLuint m_Handle{0};
    GLenum m_Stage{GL_INVALID_VALUE};
};

using ShaderHandle = RefCountPtr<GLShader>;

ShaderHandle CreateShader(const std::string& fileName);
ShaderHandle CreateShader(GLenum stage, const std::string& text, const std::string& debugName = "");

std::string ReadShaderFile(const std::string& fileName);
GLenum GLShaderStageFromFileName(const std::string& fileName);

class GLProgram : public RefCountResource<IResource>
{
public:
    GLProgram() = default;
    GLProgram(const ShaderHandle& a);
    GLProgram(const ShaderHandle& a, const ShaderHandle& b);

    ~GLProgram();

    void Create(const ShaderHandle& a, const ShaderHandle& b);
    void Destroy();

    void Use() const;

    GLuint m_Handle{0};
};

using ProgramHandle = RefCountPtr<GLProgram>;

ProgramHandle CreateProgram(ShaderHandle a);
ProgramHandle CreateProgram(ShaderHandle a, ShaderHandle b);

class GLFramebuffer : public RefCountResource<IResource>
{
public:
    // Render target color sampling type.
    enum class SamplerFilterType
    {
        Nearest,
        Linear,
    };

    GLFramebuffer() = default;
    GLFramebuffer(u32 width, u32 height, GLenum formatColor, GLenum formatDepth,
                  SamplerFilterType filterType = SamplerFilterType::Linear);

    GLFramebuffer(u32 width, u32 height, GLenum formatColor, GLenum formatDepth,
                  u32 numColorAttachments,
                  SamplerFilterType filterType = SamplerFilterType::Linear);

    ~GLFramebuffer();

    void Create(u32 width, u32 height, GLenum formatColor, GLenum formatDepth,
                SamplerFilterType filterType);
    void Destroy();

    void Bind();
    void Unbind();

    u32 m_Width{0};
    u32 m_Height{0};
    GLuint m_Handle{0};

    TextureHandle attachmentColor;
    TextureHandle attachmentDepth;

    std::vector<TextureHandle> attachmentColors;
};

using FramebufferHandle = RefCountPtr<GLFramebuffer>;

FramebufferHandle CreateFramebuffer(u32 width, u32 height, GLenum formatColor, GLenum formatDepth,
                                    GLFramebuffer::SamplerFilterType filterType
                                    = GLFramebuffer::SamplerFilterType::Linear);

FramebufferHandle CreateFramebuffer(u32 width, u32 height, GLenum formatColor, GLenum formatDepth,
                                    u32 numColorAttachments,
                                    GLFramebuffer::SamplerFilterType filterType
                                    = GLFramebuffer::SamplerFilterType::Linear);

} // namespace Nerine