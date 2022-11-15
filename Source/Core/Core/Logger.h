#pragma once

#include <mutex>
#include <ostream>
#include <sstream>
#include <string>

namespace Nerine
{

class Logger
{
public:
    enum class LEVEL
    {
        FATAL = 0,
        ERROR = 1,
        WARN = 2,
        INFO = 3,
        DEBUG = 4,
        TRACE = 5
    };

    ~Logger() = default;

    Logger(const Logger&) = delete;
    void operator=(const Logger&) = delete;
    Logger(Logger&&) noexcept = default;
    Logger& operator=(Logger&&) noexcept = default;

    static Logger& GetInstance();
    void SetOutput(std::ostream* output);

    template <typename... Args> void Log(LEVEL level, Args&&... args)
    {
        // XXX: use std::format when available
        std::ostringstream oss;
        oss << "[";

        switch (level)
        {
        case (LEVEL::FATAL):
            oss << "FATAL";
            break;
        case (LEVEL::ERROR):
            oss << "ERROR";
            break;
        case (LEVEL::WARN):
            oss << "WARN";
            break;
        case (LEVEL::INFO):
            oss << "INFO";
            break;
        case (LEVEL::DEBUG):
            oss << "DEBUG";
            break;
        case (LEVEL::TRACE):
            oss << "TRACE";
            break;
        default:
            return;
        }

        oss << "] ";
        (oss << ... << std::forward<Args>(args));

        auto&& lock __attribute__((unused)) = std::lock_guard<std::mutex>(m_OutputMutex);
        if (m_OutputStream != nullptr)
        {
            (*m_OutputStream) << oss.str() << std::endl;
        }
    };

private:
    Logger() = default;

    static std::ostream* m_OutputStream;
    static std::mutex m_OutputMutex;
};

#define LOG_SET_OUTPUT(output) Logger::GetInstance().SetOutput(output)

#define LOG_FATAL(...) Logger::GetInstance().Log(Logger::LEVEL::FATAL, __VA_ARGS__)

#define LOG_ERROR(...) Logger::GetInstance().Log(Logger::LEVEL::ERROR, __VA_ARGS__)

#define LOG_WARN(...) Logger::GetInstance().Log(Logger::LEVEL::WARN, __VA_ARGS__)

#define LOG_INFO(...) Logger::GetInstance().Log(Logger::LEVEL::INFO, __VA_ARGS__)

#define LOG_DEBUG(...) Logger::GetInstance().Log(Logger::LEVEL::DEBUG, __VA_ARGS__)

#define LOG_TRACE(...) Logger::GetInstance().Log(Logger::LEVEL::TRACE, __VA_ARGS__)

} // namespace Nerine