#include "GLDevice.h"

namespace Nerine
{

GLDevice::GLDevice()
{
    m_VendorName = (const char*)glGetString(GL_VENDOR);
    m_GPUName = (const char*)glGetString(GL_RENDERER);
}

} // namespace Nerine