#include "GLResources.h"

#include "RenderUtils.h"

#include <Core/Logger.h>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <gli/gli.hpp>
#include <gli/load_ktx.hpp>
#include <gli/texture2d.hpp>

namespace fs = std::filesystem;

namespace Nerine
{

GLBuffer::GLBuffer(GLsizeiptr size, const void* data, GLbitfield flags)
{
    Create(size, data, flags);
}

GLBuffer::~GLBuffer()
{
    if (m_Handle != 0)
    {
        Destroy();
    }
}

void GLBuffer::Create(GLsizeiptr size, const void* data, GLbitfield flags)
{
    glCreateBuffers(1, &m_Handle);
    glNamedBufferStorage(m_Handle, size, data, flags);
}

void GLBuffer::Destroy()
{
    glDeleteBuffers(1, &m_Handle);
    m_Handle = 0;
}

GLShader::GLShader(const std::string& fileName)
{
    Create(fileName);
}

GLShader::GLShader(GLenum stage, const std::string& text, const std::string& debugName)
{
    Create(stage, text, debugName);
}

GLShader::~GLShader()
{
    if (m_Handle != 0)
    {
        Destroy();
    }
}

void GLShader::Create(const std::string& fileName)
{
    return Create(GLShaderStageFromFileName(fileName), ReadShaderFile(fileName));
}

void GLShader::Create(GLenum stage, const std::string& text, const std::string& debugName)
{
    m_Handle = glCreateShader(stage);

    auto source = text.c_str();
    glShaderSource(m_Handle, 1, &source, nullptr);
    glCompileShader(m_Handle);

    char buffer[8192];
    GLsizei length = 0;
    glGetShaderInfoLog(m_Handle, sizeof(buffer), &length, buffer);

    if (length != 0)
    {
        LOG_ERROR("Error compiling shader: \n", buffer, "\nShader source:\n", text);
        return;
    }

    m_Stage = stage;
}

void GLShader::Destroy()
{
    glDeleteShader(m_Handle);
    m_Handle = 0;
    m_Stage = GL_INVALID_VALUE;
}

BufferHandle CreateBuffer(GLsizeiptr size, const void* data, GLbitfield flags)
{
    return BufferHandle::Create(new GLBuffer(size, data, flags));
}

TextureHandle CreateTexture(GLenum type, const std::string& fileName, GLenum clamp)
{
    return TextureHandle::Create(new GLTexture(type, fileName, clamp));
}

TextureHandle CreateTexture(GLenum type, u32 width, u32 height, GLenum internalFormat)
{
    return TextureHandle::Create(new GLTexture(type, width, height, internalFormat));
}

TextureHandle CreateTexture2D(u32 width, u32 height, const void* data)
{
    return TextureHandle::Create(new GLTexture(width, height, data));
}

ShaderHandle CreateShader(const std::string& fileName)
{
    return ShaderHandle::Create(new GLShader(fileName));
}

ShaderHandle CreateShader(GLenum stage, const std::string& text, const std::string& debugName)
{
    return ShaderHandle::Create(new GLShader(stage, text, debugName));
}

std::string ReadShaderFile(const std::string& filePath)
{
    std::ifstream file(filePath);

    if (!file.is_open())
    {
        LOG_ERROR("Failed to read shader file: ", fs::absolute(filePath));
        return std::string();
    }

    std::string code((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    while (code.find("#include ") != code.npos)
    {
        const auto pos = code.find("#include ");

        const auto p1 = code.find('"', pos);

        // Skip over length of '#include ' + 1 for first quotation mark.
        const auto p2 = code.find('"', pos + 10);

        if (p1 == code.npos || p2 == code.npos || p2 <= p1)
        {
            LOG_ERROR("Failed loading shader program: ", p1, " ", p2, " ", code);

            file.close();
            return std::string();
        }
        const std::string name = code.substr(p1 + 1, p2 - p1 - 1);

        // XXX: read from relative address of current shader file.
        const std::string include = ReadShaderFile(name.c_str());

        code.replace(pos, p2 - pos + 1, include.c_str());
    }

    file.close();
    return code;
}

GLenum GLShaderStageFromFileName(const std::string& filePath)
{
    auto fileName = filePath;
    if (filePath.ends_with(".glsl"))
    {
        fileName = filePath.substr(0, filePath.size() - 5);
    }

    if (fileName.ends_with(".vert") || fileName.ends_with(".vs"))
        return GL_VERTEX_SHADER;
    if (fileName.ends_with(".frag") || fileName.ends_with(".fs"))
        return GL_FRAGMENT_SHADER;
    if (fileName.ends_with(".geom") || fileName.ends_with(".gs"))
        return GL_GEOMETRY_SHADER;
    if (fileName.ends_with(".comp") || fileName.ends_with(".cs"))
        return GL_COMPUTE_SHADER;
    if (fileName.ends_with(".tesc") || fileName.ends_with(".tc"))
        return GL_TESS_CONTROL_SHADER;
    if (fileName.ends_with(".tese") || fileName.ends_with(".te"))
        return GL_TESS_EVALUATION_SHADER;

    return GL_INVALID_VALUE;
}

ProgramHandle CreateProgram(ShaderHandle a)
{
    return ProgramHandle::Create(new GLProgram(a));
}

ProgramHandle CreateProgram(ShaderHandle a, ShaderHandle b)
{
    return ProgramHandle::Create(new GLProgram(a, b));
}

FramebufferHandle CreateFramebuffer(u32 width, u32 height, GLenum formatColor, GLenum formatDepth)
{
    return FramebufferHandle::Create(new GLFramebuffer(width, height, formatColor, formatDepth));
}

GLProgram::GLProgram(const ShaderHandle& a)
{
    m_Handle = glCreateProgram();

    glAttachShader(m_Handle, a->m_Handle);
    glLinkProgram(m_Handle);
}

GLProgram::GLProgram(const ShaderHandle& a, const ShaderHandle& b)
{
    Create(a, b);
}

GLProgram::~GLProgram()
{
    if (m_Handle != 0)
    {
        Destroy();
    }
}

void GLProgram::Create(const ShaderHandle& a, const ShaderHandle& b)
{
    m_Handle = glCreateProgram();

    glAttachShader(m_Handle, a->m_Handle);
    glAttachShader(m_Handle, b->m_Handle);
    glLinkProgram(m_Handle);

    GLint isLinked = false;
    glGetProgramiv(m_Handle, GL_LINK_STATUS, &isLinked);
    if (isLinked == GL_FALSE)
    {
        GLint maxLength = 0;
        glGetProgramiv(m_Handle, GL_INFO_LOG_LENGTH, &maxLength);

        std::vector<GLchar> infoLog(maxLength);
        glGetProgramInfoLog(m_Handle, maxLength, &maxLength, &infoLog[0]);

        LOG_ERROR("Error linking program: ", infoLog.data());

        glDeleteProgram(m_Handle);
        return;
    }
}

void GLProgram::Destroy()
{
    glDeleteProgram(m_Handle);
    m_Handle = 0;
}

void GLProgram::Use() const
{
    glUseProgram(m_Handle);
}

GLFramebuffer::GLFramebuffer(u32 width, u32 height, GLenum formatColor, GLenum formatDepth)
{
    Create(width, height, formatColor, formatDepth);
}

GLFramebuffer::~GLFramebuffer()
{
    if (m_Handle != 0)
    {
        Destroy();
    }
}

void GLFramebuffer::Create(u32 width, u32 height, GLenum formatColor, GLenum formatDepth)
{
    glCreateFramebuffers(1, &m_Handle);

    if (formatColor)
    {
        attachmentColor = CreateTexture(GL_TEXTURE_2D, width, height, formatColor);

        glTextureParameteri(attachmentColor->m_Handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(attachmentColor->m_Handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glNamedFramebufferTexture(m_Handle, GL_COLOR_ATTACHMENT0, attachmentColor->m_Handle, 0);
    }
    if (formatDepth)
    {
        attachmentDepth = CreateTexture(GL_TEXTURE_2D, width, height, formatDepth);

        const float border[] = {0.0f, 0.0f, 0.0f, 0.0f};

        glTextureParameterfv(attachmentDepth->m_Handle, GL_TEXTURE_BORDER_COLOR, border);
        glTextureParameteri(attachmentDepth->m_Handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTextureParameteri(attachmentDepth->m_Handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glNamedFramebufferTexture(m_Handle, GL_DEPTH_ATTACHMENT, attachmentDepth->m_Handle, 0);
    }

    const GLenum status = glCheckNamedFramebufferStatus(m_Handle, GL_FRAMEBUFFER);

    assert(status == GL_FRAMEBUFFER_COMPLETE);

    m_Width = width;
    m_Height = height;
}

void GLFramebuffer::Destroy()
{
    Unbind();
    glDeleteFramebuffers(1, &m_Handle);
    m_Handle = 0;
    m_Width = 0;
    m_Height = 0;
}

void GLFramebuffer::Bind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_Handle);
    glViewport(0, 0, m_Width, m_Height);
}

void GLFramebuffer::Unbind()
{
    // XXX: This implicitly binds the swapchain/screen buffer.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

namespace
{

int GetNumMipMapLevels2D(int width, int height)
{
    int levels = 1;
    while ((width | height) >> levels)
        levels++;
    return levels;
}

} // namespace

GLTexture::GLTexture(GLenum type, u32 width, u32 height, GLenum internalFormat)
{
    Create(type, width, height, internalFormat);
}

GLTexture::GLTexture(u32 width, u32 height, const void* data)
{
    Create(GL_TEXTURE_2D, width, height, GL_RGBA8);
    Write2D(width, height, data);
}

GLTexture::GLTexture(GLenum type, const std::string& fileName, GLenum clamp)
{
    Load(type, fileName, clamp);
}

GLTexture::~GLTexture()
{
    if (m_Handle != 0)
    {
        Destroy();
    }
}

void GLTexture::Create(GLenum type, u32 width, u32 height, GLenum internalFormat, GLenum clamp)
{
    glCreateTextures(type, 1, &m_Handle);

    glTextureParameteri(m_Handle, GL_TEXTURE_MAX_LEVEL, 0);
    glTextureParameteri(m_Handle, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(m_Handle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTextureParameteri(m_Handle, GL_TEXTURE_WRAP_S, clamp);
    glTextureParameteri(m_Handle, GL_TEXTURE_WRAP_T, clamp);

    glTextureStorage2D(m_Handle, GetNumMipMapLevels2D(width, height), internalFormat, width,
                       height);
}

void GLTexture::Write2D(u32 width, u32 height, const void* data)
{
    assert(m_Handle != 0);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    auto numMipMaps = GetNumMipMapLevels2D(width, height);
    glTextureStorage2D(m_Handle, numMipMaps, GL_RGBA8, width, height);
    glTextureSubImage2D(m_Handle, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateTextureMipmap(m_Handle);

    glTextureParameteri(m_Handle, GL_TEXTURE_MAX_LEVEL, numMipMaps - 1);
    glTextureParameteri(m_Handle, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(m_Handle, GL_TEXTURE_MAX_ANISOTROPY, 16);

    m_HandleBindless = glGetTextureHandleARB(m_Handle);
    glMakeTextureHandleResidentARB(m_HandleBindless);
}

void GLTexture::Load(GLenum type, const std::string& fileName, GLenum clamp)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glCreateTextures(type, 1, &m_Handle);
    glTextureParameteri(m_Handle, GL_TEXTURE_MAX_LEVEL, 0);
    glTextureParameteri(m_Handle, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(m_Handle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(m_Handle, GL_TEXTURE_WRAP_S, clamp);
    glTextureParameteri(m_Handle, GL_TEXTURE_WRAP_T, clamp);

    int w = 0;
    int h = 0;
    int numMipMaps = 0;

    switch (type)
    {
    case GL_TEXTURE_2D: {
        if (fileName.ends_with(".ktx"))
        {
            gli::texture gliTex = gli::load_ktx(fileName);
            gli::gl GL(gli::gl::PROFILE_KTX);
            gli::gl::format const format = GL.translate(gliTex.format(), gliTex.swizzles());
            glm::tvec3<GLsizei> extent(gliTex.extent(0));
            w = extent.x;
            h = extent.y;
            numMipMaps = GetNumMipMapLevels2D(w, h);
            glTextureStorage2D(m_Handle, numMipMaps, format.Internal, w, h);
            glTextureSubImage2D(m_Handle, 0, 0, 0, w, h, format.External, format.Type,
                                gliTex.data(0, 0, 0));
        }
        else
        {
            u8* data = stbi_load(fileName.c_str(), &w, &h, nullptr, STBI_rgb_alpha);
            if (!data)
            {
                LOG_ERROR("Failed to load image file: ", fileName);
                Destroy();
                return;
            }

            numMipMaps = GetNumMipMapLevels2D(w, h);
            glTextureStorage2D(m_Handle, numMipMaps, GL_RGBA8, w, h);
            glTextureSubImage2D(m_Handle, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, data);
            stbi_image_free((void*)data);
        }

        glGenerateTextureMipmap(m_Handle);
        glTextureParameteri(m_Handle, GL_TEXTURE_MAX_LEVEL, numMipMaps - 1);
        glTextureParameteri(m_Handle, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameteri(m_Handle, GL_TEXTURE_MAX_ANISOTROPY, 16);
        break;
    }
    case GL_TEXTURE_CUBE_MAP: {
        int comp = 0;
        const float* data = stbi_loadf(fileName.c_str(), &w, &h, &comp, 3);

        Bitmap in(w, h, comp, BitmapFormat::Float, data);
        const bool isEquirectangular = w == 2 * h;
        Bitmap out = isEquirectangular ? ConvertEquirectangularMapToVerticalCross(in) : in;
        stbi_image_free((void*)data);
        Bitmap cubemap = ConvertVerticalCrossToCubeMapFaces(out);

        const int numMipmaps = GetNumMipMapLevels2D(cubemap.w_, cubemap.h_);

        glTextureParameteri(m_Handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_Handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_Handle, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_Handle, GL_TEXTURE_BASE_LEVEL, 0);
        glTextureParameteri(m_Handle, GL_TEXTURE_MAX_LEVEL, numMipmaps - 1);
        glTextureParameteri(m_Handle, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameteri(m_Handle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

        glTextureStorage2D(m_Handle, numMipmaps, GL_RGB32F, cubemap.w_, cubemap.h_);

        const u8* cubemapData = cubemap.data_.data();

        for (unsigned i = 0; i != 6; ++i)
        {
            glTextureSubImage3D(m_Handle, 0, 0, 0, i, cubemap.w_, cubemap.h_, 1, GL_RGB, GL_FLOAT,
                                cubemapData);
            cubemapData += cubemap.w_ * cubemap.h_ * cubemap.comp_
                           * Bitmap::GetBytesPerComponent(cubemap.fmt_);
        }

        glGenerateTextureMipmap(m_Handle);
        break;
    }
    default: {
        LOG_ERROR("GLTexture load: unsupported texture type ", type);
        Destroy();
        return;
    }
    }

    m_HandleBindless = glGetTextureHandleARB(m_Handle);
    glMakeTextureHandleResidentARB(m_HandleBindless);
}

void GLTexture::Destroy()
{
    if (m_HandleBindless != 0)
        glMakeTextureHandleNonResidentARB(m_HandleBindless);
    glDeleteTextures(1, &m_Handle);
}

} // namespace Nerine