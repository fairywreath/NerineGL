#include "Logger.h"

namespace Nerine
{

std::ostream* Logger::m_OutputStream = nullptr;
std::mutex Logger::m_OutputMutex;

void Logger::SetOutput(std::ostream* output)
{
    auto&& lock __attribute__((unused)) = std::lock_guard<std::mutex>(m_OutputMutex);

    m_OutputStream = output;
}

Logger& Logger::GetInstance()
{
    static auto&& logger = Logger();
    return (logger);
}

} // namespace Nerine
