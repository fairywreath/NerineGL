#pragma once

#include <glad/glad.h>

#include <Core/Types.h>

#include <string>

namespace Nerine
{

class GLDevice
{
public:
    GLDevice();

    [[nodiscard]] std::string GetVendorName()
    {
        return m_VendorName;
    }

    [[nodiscard]] std::string GetGPUName()
    {
        return m_GPUName;
    }

private:
    std::string m_VendorName;
    std::string m_GPUName;
};

} // namespace Nerine